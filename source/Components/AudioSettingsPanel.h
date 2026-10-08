#pragma once

#if JucePlugin_Build_Standalone

#include <JuceHeader.h>
#include "../Helpers/CustomLookAndFeel.h"
#include "../Helpers/InterfaceDefines.h"
#include "../Helpers/BorealConstants.h"

class AudioSettingsPanel : public Component,
                           public Button::Listener
{
public:
    AudioSettingsPanel (AudioDeviceManager& dm)
        : deviceManager (dm),
          selectorComp (dm, 0, 2, 0, 2, true, true, false, true)
    {
        selectorComp.setLookAndFeel (&settingsLF);
        addAndMakeVisible (selectorComp);

        closeBtn.setButtonText ("CLOSE");
        closeBtn.setClickingTogglesState (false);
        closeBtn.setRepaintsOnMouseActivity (true);
        closeBtn.addListener (this);
        addAndMakeVisible (closeBtn);

        setSize (juce::roundToInt (760.0f * BorealZoom::uiScale),
                 juce::roundToInt (648.0f * BorealZoom::uiScale));
    }

    ~AudioSettingsPanel() override
    {
        selectorComp.setLookAndFeel (nullptr);
    }

    void paint (Graphics& g) override
    {
        g.fillAll (BorealColors::background);

        auto titleArea = getLocalBounds().removeFromTop (34);
        g.setColour (GUI::Color::KeyDown);
        g.setFont (CustomLookAndFeel::makeFont (18.0f));
        g.drawText (">> AUDIO / MIDI SETTINGS",
                    titleArea.reduced ((int) GUI::Layout::ContentInset, 6),
                    Justification::centredLeft, false);

        g.setColour (GUI::Color::Accent.withAlpha (0.15f));
        g.drawLine (0.0f, 33.0f, (float) getWidth(), 33.0f, 1.0f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        bounds.removeFromTop (34);

        auto bottomBar = bounds.removeFromBottom (46);
        closeBtn.setBounds (bottomBar.withSizeKeepingCentre (140, 26));

        selectorComp.setBounds (bounds.reduced (8, 4));
    }

private:
    void buttonClicked (Button*) override
    {
        if (auto* dw = findParentComponentOfClass<DocumentWindow>())
            dw->closeButtonPressed();
    }

    struct SettingsLookAndFeel : public CustomLookAndFeel
    {
        juce::Font getComboBoxFont (juce::ComboBox&) override
        {
            return getFont (19.0f);
        }

        juce::Font getTextButtonFont (juce::TextButton&, int height) override
        {
            return getFont (jmin (18.0f, (float) height * 0.85f));
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                             bool shouldDrawButtonAsHighlighted, bool) override
        {
            using namespace BorealColors;

            const bool isOn = button.getToggleState();
            const auto area = button.getLocalBounds().toFloat();

            if (button.getButtonText() == "X")
            {
                g.setFont (getCustomFont (18.0f));
                g.setColour (shouldDrawButtonAsHighlighted ? textBrand : textMid);
                g.drawText ("X", area.translated (0, -2), juce::Justification::centred, false);
                return;
            }

            g.setFont (getCustomFont (18.0f));
            g.setColour (isOn ? textPrimary : (shouldDrawButtonAsHighlighted ? textPrimary : textMid));

            const juce::String text = isOn ? ("> " + button.getButtonText() + " <")
                                           : ("[ " + button.getButtonText() + " ]");

            g.drawText (text, area.translated (0, -2), juce::Justification::centred, true);
        }

        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                               bool shouldDrawButtonAsHighlighted, bool isButtonDown) override
        {
            if (! button.getButtonText().isEmpty())
            {
                drawButtonBackground (g, button, findColour (juce::TextButton::buttonColourId),
                                      shouldDrawButtonAsHighlighted, isButtonDown);

                using namespace BorealColors;
                const bool isOn = button.getToggleState();
                const auto area = button.getLocalBounds();
                g.setFont (getCustomFont (18.0f));
                g.setColour (isOn ? textPrimary : (shouldDrawButtonAsHighlighted ? textPrimary : textMid));

                const juce::String text = isOn ? ("> " + button.getButtonText() + " <")
                                               : ("[ " + button.getButtonText() + " ]");

                g.drawText (text, area.translated (0, -2), juce::Justification::centred, true);
            }
            else
            {
                LookAndFeel_V4::drawToggleButton (g, button, shouldDrawButtonAsHighlighted, isButtonDown);
            }
        }

        void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
        {
            label.setBounds (0, 0, box.getWidth() - 20, box.getHeight() - 7);
            label.setFont (getCustomFont (17.0f));
            label.setJustificationType (juce::Justification::centred);
            label.setColour (juce::Label::textColourId, BorealColors::textPrimary);
        }
    };

    AudioDeviceManager& deviceManager;
    SettingsLookAndFeel settingsLF;
    AudioDeviceSelectorComponent selectorComp;
    TextButton closeBtn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioSettingsPanel)
};

class SettingsWindow : public DocumentWindow
{
public:
    SettingsWindow (AudioDeviceManager& dm)
        : DocumentWindow ("Audio/MIDI Settings",
                          BorealColors::menuBg,
                          allButtons)
    {
        setContentOwned (new AudioSettingsPanel (dm), true);
        setResizable (false, false);
        centreWithSize (juce::roundToInt (760.0f * BorealZoom::uiScale),
                        juce::roundToInt (648.0f * BorealZoom::uiScale));
        setVisible (true);
    }

    void closeButtonPressed() override { delete this; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsWindow)
};

#endif
