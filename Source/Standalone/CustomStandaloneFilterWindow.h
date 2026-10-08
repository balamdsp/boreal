/*
  ==============================================================================

   This file is part of the JUCE framework.
   Copyright (c) Raw Material Software Limited

   JUCE is an open source framework subject to commercial or open source
   licensing.

   By downloading, installing, or using the JUCE framework, or combining the
   JUCE framework with any other source code, object code, content or any other
   copyrightable work, you agree to the terms of the JUCE End User Licence
   Agreement, and all incorporated terms including the JUCE Privacy Policy and
   the JUCE Website Terms of Service, as applicable, which will bind you. If you
   do not agree to the terms of these agreements, we will not license the JUCE
   framework to you, and you must discontinue the installation or download
   process and cease use of the JUCE framework.

   JUCE End User Licence Agreement: https://juce.com/legal/juce-9-licence/
   JUCE Privacy Policy: https://juce.com/juce-privacy-policy
   JUCE Website Terms of Service: https://juce.com/juce-website-terms-of-service/

   Or:

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   THE JUCE FRAMEWORK IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

#pragma once

#include "../Helpers/BorealConstants.h"

#ifndef DOXYGEN
 #include <juce_audio_plugin_client/detail/juce_CreatePluginFilter.h>
#endif

namespace juce
{

class BorealPluginHolder    : private AudioIODeviceCallback,
                                  private Timer,
                                  private Value::Listener
{
public:

    struct PluginInOuts   { short numIns, numOuts; };

    BorealPluginHolder (PropertySet* settingsToUse,
                            bool takeOwnershipOfSettings = true,
                            const String& preferredDefaultDeviceName = String(),
                            const AudioDeviceManager::AudioDeviceSetup* preferredSetupOptions = nullptr,
                            const Array<PluginInOuts>& channels = Array<PluginInOuts>(),
                           #if JUCE_ANDROID || JUCE_IOS
                            bool shouldAutoOpenMidiDevices = true
                           #else
                            bool shouldAutoOpenMidiDevices = false
                           #endif
                            )

        : settings (settingsToUse, takeOwnershipOfSettings),
          channelConfiguration (channels),
          autoOpenMidiDevices (shouldAutoOpenMidiDevices)
    {
        jassert (currentInstance == nullptr);
        currentInstance = this;

        shouldMuteInput.addListener (this);
        shouldMuteInput = ! isInterAppAudioConnected();

        handleCreatePlugin();

        auto inChannels = (channelConfiguration.size() > 0 ? channelConfiguration[0].numIns
                                                           : processor->getMainBusNumInputChannels());

        if (preferredSetupOptions != nullptr)
            options.reset (new AudioDeviceManager::AudioDeviceSetup (*preferredSetupOptions));

        auto audioInputRequired = (inChannels > 0);

        if (audioInputRequired && RuntimePermissions::isRequired (RuntimePermissions::recordAudio)
            && ! RuntimePermissions::isGranted (RuntimePermissions::recordAudio))
            RuntimePermissions::request (RuntimePermissions::recordAudio,
                                         [this, preferredDefaultDeviceName] (bool granted) { init (granted, preferredDefaultDeviceName); });
        else
            init (audioInputRequired, preferredDefaultDeviceName);
    }

    void init (bool enableAudioInput, const String& preferredDefaultDeviceName)
    {
        setupAudioDevices (enableAudioInput, preferredDefaultDeviceName, options.get());
        reloadPluginState();
        startPlaying();

       if (autoOpenMidiDevices)
           startTimer (500);
    }

    ~BorealPluginHolder() override
    {
        stopTimer();

        handleDeletePlugin();
        shutDownAudioDevices();

        currentInstance = nullptr;
    }

    virtual void createPlugin()
    {
        handleCreatePlugin();
    }

    virtual void deletePlugin()
    {
        handleDeletePlugin();
    }

    int getNumInputChannels() const
    {
        if (processor == nullptr)
            return 0;

        return (channelConfiguration.size() > 0 ? channelConfiguration[0].numIns
                                                : processor->getMainBusNumInputChannels());
    }

    int getNumOutputChannels() const
    {
        if (processor == nullptr)
            return 0;

        return (channelConfiguration.size() > 0 ? channelConfiguration[0].numOuts
                                                : processor->getMainBusNumOutputChannels());
    }

    static String getFilePatterns (const String& fileSuffix)
    {
        if (fileSuffix.isEmpty())
            return {};

        return (fileSuffix.startsWithChar ('.') ? "*" : "*.") + fileSuffix;
    }

    Value& getMuteInputValue()                           { return shouldMuteInput; }
    bool getProcessorHasPotentialFeedbackLoop() const    { return processorHasPotentialFeedbackLoop; }
    void valueChanged (Value& value) override            { muteInput = (bool) value.getValue(); }

    File getLastFile() const
    {
        File f;

        if (settings != nullptr)
            f = File (settings->getValue ("lastStateFile"));

        if (f == File())
            f = File::getSpecialLocation (File::userDocumentsDirectory);

        return f;
    }

    void setLastFile (const FileChooser& fc)
    {
        if (settings != nullptr)
            settings->setValue ("lastStateFile", fc.getResult().getFullPathName());
    }

    void askUserToSaveState (const String& fileSuffix = String())
    {
        stateFileChooser = std::make_unique<FileChooser> (TRANS ("Save current state"),
                                                          getLastFile(),
                                                          getFilePatterns (fileSuffix));
        auto flags = FileBrowserComponent::saveMode
                   | FileBrowserComponent::canSelectFiles
                   | FileBrowserComponent::warnAboutOverwriting;

        stateFileChooser->launchAsync (flags, [this] (const FileChooser& fc)
        {
            if (fc.getResult() == File{})
                return;

            setLastFile (fc);

            MemoryBlock data;
            processor->getStateInformation (data);

            if (! fc.getResult().replaceWithData (data.getData(), data.getSize()))
            {
                auto opts = MessageBoxOptions::makeOptionsOk (AlertWindow::WarningIcon,
                                                              TRANS ("Error whilst saving"),
                                                              TRANS ("Couldn't write to the specified file!"));
                messageBox = AlertWindow::showScopedAsync (opts, nullptr);
            }
        });
    }

    void askUserToLoadState (const String& fileSuffix = String())
    {
        stateFileChooser = std::make_unique<FileChooser> (TRANS ("Load a saved state"),
                                                          getLastFile(),
                                                          getFilePatterns (fileSuffix));
        auto flags = FileBrowserComponent::openMode
                   | FileBrowserComponent::canSelectFiles;

        stateFileChooser->launchAsync (flags, [this] (const FileChooser& fc)
        {
            if (fc.getResult() == File{})
                return;

            setLastFile (fc);

            MemoryBlock data;

            if (fc.getResult().loadFileAsData (data))
            {
                processor->setStateInformation (data.getData(), (int) data.getSize());
            }
            else
            {
                auto opts = MessageBoxOptions::makeOptionsOk (AlertWindow::WarningIcon,
                                                              TRANS ("Error whilst loading"),
                                                              TRANS ("Couldn't read from the specified file!"));
                messageBox = AlertWindow::showScopedAsync (opts, nullptr);
            }
        });
    }

    void startPlaying()
    {
        player.setProcessor (processor.get());

       #if JucePlugin_Enable_IAA && JUCE_IOS
        if (auto device = dynamic_cast<iOSAudioIODevice*> (deviceManager.getCurrentAudioDevice()))
        {
            processor->setPlayHead (device->getAudioPlayHead());
            device->setMidiMessageCollector (&player.getMidiMessageCollector());
        }
       #endif
    }

    void stopPlaying()
    {
        player.setProcessor (nullptr);
    }

    void showAudioSettingsDialog()
    {
        DialogWindow::LaunchOptions o;

        int maxNumInputs = 0, maxNumOutputs = 0;

        if (channelConfiguration.size() > 0)
        {
            auto& defaultConfig = channelConfiguration.getReference (0);

            maxNumInputs  = jmax (0, (int) defaultConfig.numIns);
            maxNumOutputs = jmax (0, (int) defaultConfig.numOuts);
        }

        if (auto* bus = processor->getBus (true, 0))
            maxNumInputs = jmax (0, bus->getDefaultLayout().size());

        if (auto* bus = processor->getBus (false, 0))
            maxNumOutputs = jmax (0, bus->getDefaultLayout().size());

        auto content = std::make_unique<SettingsComponent> (*this, deviceManager, maxNumInputs, maxNumOutputs);
        content->setSize (juce::roundToInt (500.0f * BorealZoom::uiScale),
                          juce::roundToInt (550.0f * BorealZoom::uiScale));
        content->setToRecommendedSize();

        o.content.setOwned (content.release());

        o.dialogTitle                   = TRANS ("Audio/MIDI Settings");
        o.dialogBackgroundColour        = o.content->getLookAndFeel().findColour (ResizableWindow::backgroundColourId);
        o.escapeKeyTriggersCloseButton  = true;
        o.useNativeTitleBar             = true;
        o.resizable                     = false;

        o.launchAsync();
    }

    void saveAudioDeviceState()
    {
        if (settings != nullptr)
        {
            auto xml = deviceManager.createStateXml();

            settings->setValue ("audioSetup", xml.get());

           #if ! (JUCE_IOS || JUCE_ANDROID)
            settings->setValue ("shouldMuteInput", (bool) shouldMuteInput.getValue());
           #endif
        }
    }

    void reloadAudioDeviceState (bool enableAudioInput,
                                 const String& preferredDefaultDeviceName,
                                 const AudioDeviceManager::AudioDeviceSetup* preferredSetupOptions)
    {
        std::unique_ptr<XmlElement> savedState;

        if (settings != nullptr)
        {
            savedState = settings->getXmlValue ("audioSetup");

           #if ! (JUCE_IOS || JUCE_ANDROID)
            shouldMuteInput.setValue (settings->getBoolValue ("shouldMuteInput", true));
           #endif
        }

        auto inputChannels  = getNumInputChannels();
        auto outputChannels = getNumOutputChannels();

        if (inputChannels == 0 && outputChannels == 0 && processor->isMidiEffect())
        {
            outputChannels = 1;
        }

        deviceManager.initialise (enableAudioInput ? inputChannels : 0,
                                  outputChannels,
                                  savedState.get(),
                                  true,
                                  preferredDefaultDeviceName,
                                  preferredSetupOptions);
    }

    void savePluginState()
    {
        if (settings != nullptr && processor != nullptr)
        {
            MemoryBlock data;
            processor->getStateInformation (data);

            settings->setValue ("filterState", data.toBase64Encoding());
        }
    }

    void reloadPluginState()
    {
        if (settings != nullptr)
        {
            MemoryBlock data;

            if (data.fromBase64Encoding (settings->getValue ("filterState")) && data.getSize() > 0)
                processor->setStateInformation (data.getData(), (int) data.getSize());
        }
    }

    void switchToHostApplication()
    {
       #if JUCE_IOS
        if (auto device = dynamic_cast<iOSAudioIODevice*> (deviceManager.getCurrentAudioDevice()))
            device->switchApplication();
       #endif
    }

    bool isInterAppAudioConnected()
    {
       #if JUCE_IOS
        if (auto device = dynamic_cast<iOSAudioIODevice*> (deviceManager.getCurrentAudioDevice()))
            return device->isInterAppAudioConnected();
       #endif

        return false;
    }

    Image getIAAHostIcon ([[maybe_unused]] int size)
    {
       #if JUCE_IOS && JucePlugin_Enable_IAA
        if (auto device = dynamic_cast<iOSAudioIODevice*> (deviceManager.getCurrentAudioDevice()))
            return device->getIcon (size);
       #else
       #endif

        return {};
    }

    static BorealPluginHolder* getInstance()
    {
        return currentInstance;
    }

    OptionalScopedPointer<PropertySet> settings;
    std::unique_ptr<AudioProcessor> processor;
    AudioDeviceManager deviceManager;
    AudioProcessorPlayer player;
    Array<PluginInOuts> channelConfiguration;

    bool processorHasPotentialFeedbackLoop = true;
    std::atomic<bool> muteInput { true };
    Value shouldMuteInput;
    AudioBuffer<float> emptyBuffer;
    bool autoOpenMidiDevices;

    std::unique_ptr<AudioDeviceManager::AudioDeviceSetup> options;
    Array<MidiDeviceInfo> lastMidiDevices;

    std::unique_ptr<FileChooser> stateFileChooser;
    ScopedMessageBox messageBox;

private:
    inline static BorealPluginHolder* currentInstance = nullptr;

    void handleCreatePlugin()
    {
        processor = createPluginFilterOfType (AudioProcessor::wrapperType_Standalone);
        processor->disableNonMainBuses();
        processor->setRateAndBufferSizeDetails (44100, 512);

        processorHasPotentialFeedbackLoop = (getNumInputChannels() > 0 && getNumOutputChannels() > 0);
    }

    void handleDeletePlugin()
    {
        stopPlaying();
        processor = nullptr;
    }

    class SettingsComponent : public Component
    {
    public:
        SettingsComponent (BorealPluginHolder& pluginHolder,
                           AudioDeviceManager& deviceManagerToUse,
                           int maxAudioInputChannels,
                           int maxAudioOutputChannels)
            : owner (pluginHolder),
              deviceSelector (deviceManagerToUse,
                              0, maxAudioInputChannels,
                              0, maxAudioOutputChannels,
                              true,
                              (pluginHolder.processor.get() != nullptr && pluginHolder.processor->producesMidi()),
                              true, false),
              shouldMuteLabel  ("Feedback Loop:", "Feedback Loop:"),
              shouldMuteButton ("Mute audio input")
        {
            setOpaque (true);

            shouldMuteButton.setClickingTogglesState (true);
            shouldMuteButton.getToggleStateValue().referTo (owner.shouldMuteInput);

            addAndMakeVisible (deviceSelector);

            if (owner.getProcessorHasPotentialFeedbackLoop())
            {
                addAndMakeVisible (shouldMuteButton);
                addAndMakeVisible (shouldMuteLabel);

                shouldMuteLabel.attachToComponent (&shouldMuteButton, true);
            }
        }

        void paint (Graphics& g) override
        {
            g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
        }

        void resized() override
        {
            const ScopedValueSetter<bool> scope (isResizing, true);

            auto r = getLocalBounds();

            if (owner.getProcessorHasPotentialFeedbackLoop())
            {
                auto itemHeight = deviceSelector.getItemHeight();
                auto extra = r.removeFromTop (itemHeight);

                auto seperatorHeight = (itemHeight >> 1);
                shouldMuteButton.setBounds (Rectangle<int> (extra.proportionOfWidth (0.35f), seperatorHeight,
                                                            extra.proportionOfWidth (0.60f), deviceSelector.getItemHeight()));

                r.removeFromTop (seperatorHeight);
            }

            deviceSelector.setBounds (r);
        }

        void childBoundsChanged (Component* childComp) override
        {
            if (! isResizing && childComp == &deviceSelector)
                setToRecommendedSize();
        }

        void setToRecommendedSize()
        {
            const auto extraHeight = [&]
            {
                if (! owner.getProcessorHasPotentialFeedbackLoop())
                    return 0;

                const auto itemHeight = deviceSelector.getItemHeight();
                const auto separatorHeight = (itemHeight >> 1);
                return itemHeight + separatorHeight;
            }();

            setSize (getWidth(), deviceSelector.getHeight() + extraHeight);
        }

    private:
        BorealPluginHolder& owner;
        AudioDeviceSelectorComponent deviceSelector;
        Label shouldMuteLabel;
        ToggleButton shouldMuteButton;
        bool isResizing = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsComponent)
    };

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const AudioIODeviceCallbackContext& context) override
    {
        if (muteInput)
        {
            emptyBuffer.clear();
            inputChannelData = emptyBuffer.getArrayOfReadPointers();
        }

        player.audioDeviceIOCallbackWithContext (inputChannelData,
                                                 numInputChannels,
                                                 outputChannelData,
                                                 numOutputChannels,
                                                 numSamples,
                                                 context);
    }

    void audioDeviceAboutToStart (AudioIODevice* device) override
    {
        emptyBuffer.setSize (device->getActiveInputChannels().countNumberOfSetBits(), device->getCurrentBufferSizeSamples());
        emptyBuffer.clear();

        player.audioDeviceAboutToStart (device);
        player.setMidiOutput (deviceManager.getDefaultMidiOutput());
    }

    void audioDeviceStopped() override
    {
        player.setMidiOutput (nullptr);
        player.audioDeviceStopped();
        emptyBuffer.setSize (0, 0);
    }

    void setupAudioDevices (bool enableAudioInput,
                            const String& preferredDefaultDeviceName,
                            const AudioDeviceManager::AudioDeviceSetup* preferredSetupOptions)
    {
        deviceManager.addAudioCallback (this);
        deviceManager.addMidiInputDeviceCallback ({}, &player);

        reloadAudioDeviceState (enableAudioInput, preferredDefaultDeviceName, preferredSetupOptions);
    }

    void shutDownAudioDevices()
    {
        saveAudioDeviceState();

        deviceManager.removeMidiInputDeviceCallback ({}, &player);
        deviceManager.removeAudioCallback (this);
    }

    void timerCallback() override
    {
        auto newMidiDevices = MidiInput::getAvailableDevices();

        if (newMidiDevices != lastMidiDevices)
        {
            for (auto& oldDevice : lastMidiDevices)
                if (! newMidiDevices.contains (oldDevice))
                    deviceManager.setMidiInputDeviceEnabled (oldDevice.identifier, false);

            for (auto& newDevice : newMidiDevices)
                if (! lastMidiDevices.contains (newDevice))
                    deviceManager.setMidiInputDeviceEnabled (newDevice.identifier, true);

            lastMidiDevices = newMidiDevices;
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BorealPluginHolder)
};

class BorealFilterWindow    : public DocumentWindow
{
public:
    typedef BorealPluginHolder::PluginInOuts PluginInOuts;

    BorealFilterWindow (const String& title,
                            Colour backgroundColour,
                            std::unique_ptr<BorealPluginHolder> pluginHolderIn)
        : DocumentWindow (title, backgroundColour, DocumentWindow::minimiseButton | DocumentWindow::closeButton),
          pluginHolder (std::move (pluginHolderIn))
    {
        setConstrainer (&decoratorConstrainer);

       #if JUCE_IOS || JUCE_ANDROID
        setTitleBarHeight (0);
       #else
        setUsingNativeTitleBar (true);
        setTitleBarButtonsRequired (DocumentWindow::minimiseButton | DocumentWindow::closeButton, false);
       #endif

       #if JUCE_IOS || JUCE_ANDROID
        setFullScreen (true);
        updateContent();
       #else
        updateContent();

        const auto windowScreenBounds = [this]() -> Rectangle<int>
        {
            const auto width = getWidth();
            const auto height = getHeight();

            const auto& displays = Desktop::getInstance().getDisplays();

            if (displays.displays.isEmpty())
                return { width, height };

            if (auto* props = pluginHolder->settings.get())
            {
                constexpr int defaultValue = -100;

                const auto x = props->getIntValue ("windowX", defaultValue);
                const auto y = props->getIntValue ("windowY", defaultValue);

                if (x != defaultValue && y != defaultValue)
                {
                    const auto screenLimits = displays.getDisplayForRect ({ x, y, width, height })->userBounds.toNearestInt();

                    return { jlimit (screenLimits.getX(), jmax (screenLimits.getX(), screenLimits.getRight()  - width),  x),
                             jlimit (screenLimits.getY(), jmax (screenLimits.getY(), screenLimits.getBottom() - height), y),
                             width, height };
                }
            }

            const auto displayArea = displays.getPrimaryDisplay()->userBounds.toNearestInt();

            return { displayArea.getCentreX() - width / 2,
                     displayArea.getCentreY() - height / 2,
                     width, height };
        }();

        setBoundsConstrained (windowScreenBounds);

        if (! isResizable())
            setResizable (true, false);
       #endif
    }

    BorealFilterWindow (const String& title,
                            Colour backgroundColour,
                            PropertySet* settingsToUse,
                            bool takeOwnershipOfSettings,
                            const String& preferredDefaultDeviceName = String(),
                            const AudioDeviceManager::AudioDeviceSetup* preferredSetupOptions = nullptr,
                            const Array<PluginInOuts>& constrainToConfiguration = {},
                           #if JUCE_ANDROID || JUCE_IOS
                            bool autoOpenMidiDevices = true
                           #else
                            bool autoOpenMidiDevices = false
                           #endif
                            )
        : BorealFilterWindow (title,
                                  backgroundColour,
                                  std::make_unique<BorealPluginHolder> (settingsToUse,
                                                                            takeOwnershipOfSettings,
                                                                            preferredDefaultDeviceName,
                                                                            preferredSetupOptions,
                                                                            constrainToConfiguration,
                                                                            autoOpenMidiDevices))
    {
    }

    ~BorealFilterWindow() override
    {
       #if (! JUCE_IOS) && (! JUCE_ANDROID)
        if (auto* props = pluginHolder->settings.get())
        {
            props->setValue ("windowX", getX());
            props->setValue ("windowY", getY());
        }
       #endif

        pluginHolder->stopPlaying();
        clearContentComponent();
        pluginHolder = nullptr;
    }

    AudioProcessor* getAudioProcessor() const noexcept      { return pluginHolder->processor.get(); }
    AudioDeviceManager& getDeviceManager() const noexcept   { return pluginHolder->deviceManager; }

    void resetToDefaultState()
    {
        pluginHolder->stopPlaying();
        clearContentComponent();
        pluginHolder->deletePlugin();

        if (auto* props = pluginHolder->settings.get())
            props->removeValue ("filterState");

        pluginHolder->createPlugin();
        updateContent();
        pluginHolder->startPlaying();
    }

    void closeButtonPressed() override
    {
        pluginHolder->savePluginState();

        JUCEApplicationBase::quit();
    }

    virtual BorealPluginHolder* getPluginHolder()    { return pluginHolder.get(); }

    std::unique_ptr<BorealPluginHolder> pluginHolder;

private:
    void updateContent()
    {
        auto* content = new MainContentComponent (*this);
        decoratorConstrainer.setMainContentComponent (content);

       #if JUCE_IOS || JUCE_ANDROID
        constexpr auto resizeAutomatically = false;
       #else
        constexpr auto resizeAutomatically = true;
       #endif

        setContentOwned (content, resizeAutomatically);
        if (! isResizable())
            setResizable (true, false);
    }

    class MainContentComponent  : public Component,
                                  private Value::Listener,
                                  private Button::Listener,
                                  private ComponentListener
    {
    public:
        MainContentComponent (BorealFilterWindow& filterWindow)
            : owner (filterWindow), notification (this),
              editor (owner.getAudioProcessor()->hasEditor() ? owner.getAudioProcessor()->createEditorAndMakeActive()
                                                             : new GenericAudioProcessorEditor (*owner.getAudioProcessor()))
        {
            inputMutedValue.referTo (owner.pluginHolder->getMuteInputValue());

            if (editor != nullptr)
            {
                editor->addComponentListener (this);
                handleMovedOrResized();

                addAndMakeVisible (editor.get());
            }

            addChildComponent (notification);

            if (owner.pluginHolder->getProcessorHasPotentialFeedbackLoop())
            {
                inputMutedValue.addListener (this);
                shouldShowNotification = inputMutedValue.getValue();
            }

            inputMutedChanged (shouldShowNotification);
        }

        ~MainContentComponent() override
        {
            if (editor != nullptr)
            {
                editor->removeComponentListener (this);
                owner.pluginHolder->processor->editorBeingDeleted (editor.get());
                editor = nullptr;
            }
        }

        void resized() override
        {
            handleResized();
        }

        ComponentBoundsConstrainer* getEditorConstrainer() const
        {
            if (auto* e = editor.get())
                return e->getConstrainer();

            return nullptr;
        }

        BorderSize<int> computeBorder() const
        {
            const auto nativeFrame = [&]() -> BorderSize<int>
            {
                if (auto* peer = owner.getPeer())
                    if (const auto frameSize = peer->getFrameSizeIfPresent())
                        return *frameSize;

                return {};
            }();

            return nativeFrame.addedTo (owner.getContentComponentBorder())
                              .addedTo (BorderSize<int> { shouldShowNotification ? NotificationArea::height : 0, 0, 0, 0 });
        }

    private:
        class NotificationArea : public Component
        {
        public:
            enum { height = 30 };

            NotificationArea (Button::Listener* settingsButtonListener)
                : notification ("notification", "Audio input is muted to avoid feedback loop"),
                 #if JUCE_IOS || JUCE_ANDROID
                  settingsButton ("Unmute Input")
                 #else
                  settingsButton ("Settings...")
                 #endif
            {
                setOpaque (true);

                notification.setColour (Label::textColourId, Colours::black);

                settingsButton.addListener (settingsButtonListener);

                addAndMakeVisible (notification);
                addAndMakeVisible (settingsButton);
            }

            void paint (Graphics& g) override
            {
                auto r = getLocalBounds();

                g.setColour (Colours::darkgoldenrod);
                g.fillRect (r.removeFromBottom (1));

                g.setColour (Colours::lightgoldenrodyellow);
                g.fillRect (r);
            }

            void resized() override
            {
                auto r = getLocalBounds().reduced (5);

                settingsButton.setBounds (r.removeFromRight (70));
                notification.setBounds (r);
            }
        private:
            Label notification;
            TextButton settingsButton;
        };

        void inputMutedChanged (bool newInputMutedValue)
        {
            shouldShowNotification = newInputMutedValue;
            notification.setVisible (shouldShowNotification);

           #if JUCE_IOS || JUCE_ANDROID
            handleResized();
           #else
            if (editor != nullptr)
            {
                const int extraHeight = shouldShowNotification ? NotificationArea::height : 0;
                const auto rect = getSizeToContainEditor();
                setSize (rect.getWidth(), rect.getHeight() + extraHeight);
            }
           #endif
        }

        void valueChanged (Value& value) override     { inputMutedChanged (value.getValue()); }
        void buttonClicked (Button*) override
        {
           #if JUCE_IOS || JUCE_ANDROID
            owner.pluginHolder->getMuteInputValue().setValue (false);
           #else
            owner.pluginHolder->showAudioSettingsDialog();
           #endif
        }

        void handleResized()
        {
            auto r = getLocalBounds();

            if (shouldShowNotification)
                notification.setBounds (r.removeFromTop (NotificationArea::height));

            if (editor != nullptr)
            {
                const auto newPos = r.getTopLeft().toFloat().transformedBy (editor->getTransform().inverted());

                if (preventResizingEditor)
                    editor->setTopLeftPosition (newPos.roundToInt());
                else
                    editor->setBoundsConstrained (editor->getLocalArea (this, r.toFloat()).withPosition (newPos).toNearestInt());
            }
        }

        void handleMovedOrResized()
        {
            const ScopedValueSetter<bool> scope (preventResizingEditor, true);

            if (editor != nullptr)
            {
                auto rect = getSizeToContainEditor();

                setSize (rect.getWidth(),
                         rect.getHeight() + (shouldShowNotification ? NotificationArea::height : 0));
            }
        }

        void componentMovedOrResized (Component&, bool, bool) override
        {
            handleMovedOrResized();
        }

        Rectangle<int> getSizeToContainEditor() const
        {
            if (editor != nullptr)
                return getLocalArea (editor.get(), editor->getLocalBounds());

            return {};
        }

        BorealFilterWindow& owner;
        NotificationArea notification;
        std::unique_ptr<AudioProcessorEditor> editor;
        Value inputMutedValue;
        bool shouldShowNotification = false;
        bool preventResizingEditor = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
    };

    class DecoratorConstrainer : public BorderedComponentBoundsConstrainer
    {
    public:
        ComponentBoundsConstrainer* getWrappedConstrainer() const override
        {
            return contentComponent != nullptr ? contentComponent->getEditorConstrainer() : nullptr;
        }

        BorderSize<int> getAdditionalBorder() const override
        {
            return contentComponent != nullptr ? contentComponent->computeBorder() : BorderSize<int>{};
        }

        void setMainContentComponent (MainContentComponent* in) { contentComponent = in; }

    private:
        MainContentComponent* contentComponent = nullptr;
    };

    DecoratorConstrainer decoratorConstrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BorealFilterWindow)
};

}
