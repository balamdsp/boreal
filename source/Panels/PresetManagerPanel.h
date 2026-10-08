#pragma once

#include <JuceHeader.h>

#include "../Standalone/CustomStandaloneFilterWindow.h"

#include "../Components/AudioSettingsPanel.h"
#include "../Helpers/CustomLookAndFeel.h"
#include "../Helpers/InterfaceDefines.h"
#include "../Managers/BorealPresetManager.h"
#include "MorphingPanel.h"
#include "../Helpers/BorealConstants.h"
#include "../Helpers/BorealParameters.h"
#include "../PluginProcessor.h"
#include "../AboutWindow.h"

static constexpr int kLoadFromFileId = 0x1001;
static constexpr int kSearchPresetsId = 0x1002;

static constexpr int kCrtStrengthLow = 0x2000;
static constexpr int kCrtStrengthMedium = 0x2001;
static constexpr int kCrtStrengthHigh = 0x2002;
static constexpr int kZoomBaseId = 0x3000;

inline bool isInStandaloneApp (const Component* c)
{
    return c != nullptr
        && dynamic_cast<const juce::BorealFilterWindow*> (c->getTopLevelComponent()) != nullptr;
}

class HamburgerButton : public TextButton
{
public:
    using TextButton::TextButton;

    void paint (Graphics& g) override
    {
        getLookAndFeel().drawButtonBackground (g, *this, findColour (buttonColourId), isMouseOver(), isDown());

        const auto bounds = getLocalBounds().toFloat();
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float barW = 14.0f * BorealZoom::uiScale;
        const float barH = 2.0f * BorealZoom::uiScale;
        const float gap = 5.0f * BorealZoom::uiScale;

        g.setColour (isMouseOver() ? BorealColors::textPrimary : BorealColors::textMid);
        for (int i = -1; i <= 1; ++i)
            g.fillRect (cx - barW * 0.5f, cy + i * gap - barH * 0.5f, barW, barH);
    }
};

class PerfModeButton : public TextButton
{
public:
    using TextButton::TextButton;

    void paint (Graphics& g) override
    {
        getLookAndFeel().drawButtonBackground (g, *this, findColour (buttonColourId), isMouseOver(), isDown());

        const auto bounds = getLocalBounds().toFloat();
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float s = 9.0f * BorealZoom::uiScale;

        juce::Path tri;
        tri.addTriangle (cx - s * 0.4f, cy - s * 0.5f,
                         cx + s * 0.6f, cy,
                         cx - s * 0.4f, cy + s * 0.5f);

        g.setColour (isMouseOver() ? BorealColors::textPrimary : BorealColors::textMid);
        g.fillPath (tri);
    }
};

class AnaModeButton : public TextButton
{
public:
    using TextButton::TextButton;

    void paint (Graphics& g) override
    {
        getLookAndFeel().drawButtonBackground (g, *this, findColour (buttonColourId), isMouseOver(), isDown());

        const auto bounds = getLocalBounds().toFloat();
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float barW = 3.0f * BorealZoom::uiScale;
        const float gap = 3.0f * BorealZoom::uiScale;
        const float h1 = 6.0f * BorealZoom::uiScale;
        const float h2 = 12.0f * BorealZoom::uiScale;
        const float h3 = 9.0f * BorealZoom::uiScale;

        g.setColour (isMouseOver() ? BorealColors::textPrimary : BorealColors::textMid);
        g.fillRect (cx - gap - barW, cy - h1 * 0.5f, barW, h1);
        g.fillRect (cx - barW * 0.5f, cy - h2 * 0.5f, barW, h2);
        g.fillRect (cx + gap, cy - h3 * 0.5f, barW, h3);
    }
};

class PresetManagerPanel
:   public Component,
    public Button::Listener,
    public juce::Value::Listener
{
public:

    enum MenuOption
    {
        None = 0,
        Init,
        Save,
        SaveAs,
        LoadFromFile,
        SetPresetFolder,
        ResetPresetFolder,
        About,
        CrtEnabled,
        StandaloneAudioSettings,
        StandaloneSaveState,
        StandaloneLoadState,
        StandaloneReset,
        NUM_OPTIONS
    };

    PresetManagerPanel (BorealAudioProcessor* inProcessor)
    :   mPresetManager (inProcessor->getPresetManager()),
        mProcessor (inProcessor)
    {
        mPresetDisplay.setButtonText ("Untitled");
        mPresetDisplay.setClickingTogglesState (false);
        mPresetDisplay.setTooltip ("Current preset -- click to browse and load presets");
        mPresetDisplay.addListener (this);
        addAndMakeVisible (mPresetDisplay);

        mMenuButton.setClickingTogglesState (false);
        mMenuButton.setTooltip ("Preset menu -- save, folders, layout and about");
        mMenuButton.addListener (this);
        mMenuButton.setRepaintsOnMouseActivity (false);
        addAndMakeVisible (mMenuButton);

        modePerfButton.setClickingTogglesState (true);
        modePerfButton.setRadioGroupId (7777);
        modePerfButton.onClick = [this]
        { mProcessor->setUiMode ("performance"); };
        modePerfButton.setTooltip ("Performance window -- pad, slots and core controls");
        addAndMakeVisible (modePerfButton);

        modeAnaButton.setClickingTogglesState (true);
        modeAnaButton.setRadioGroupId (7777);
        modeAnaButton.onClick = [this]
        { mProcessor->setUiMode ("analyze"); };
        modeAnaButton.setTooltip ("Analyze window -- source waveform, partials map and resynthesis");
        addAndMakeVisible (modeAnaButton);

        mProcessor->uiModeValue.addListener (this);
        syncModeButtons();
    }

    ~PresetManagerPanel() override
    {
        if (mProcessor != nullptr)
            mProcessor->uiModeValue.removeListener (this);
    }

    void paint (Graphics&) override {}

    void resized() override
    {
        const float s = BorealZoom::uiScale;
        const float content_pad = GUI::Layout::ContentInset * s;

        auto bounds = getLocalBounds();

        const int icon_size = juce::roundToInt (28.0f * s);
        const int control_gap = juce::roundToInt (6.0f * s);

        const int controls_width = bounds.getWidth() - (int) (content_pad * 2.0f);
        const int controls_x = bounds.getX() + (int) content_pad;
        const int controls_y = (bounds.getHeight() - icon_size) / 2;

        int x = controls_x;
        const int preset_width = controls_width - (icon_size * 3) - (control_gap * 3);
        mPresetDisplay.setBounds (x, controls_y, preset_width, icon_size);
        x += preset_width + control_gap;

        modePerfButton.setBounds (x, controls_y, icon_size, icon_size);
        x += icon_size + control_gap;

        modeAnaButton.setBounds (x, controls_y, icon_size, icon_size);
        x += icon_size + control_gap;

        mMenuButton.setBounds (x, controls_y, icon_size, icon_size);
    }

private:

    void valueChanged (juce::Value& value) override
    {
        if (value.refersToSameSourceAs (mProcessor->uiModeValue))
            syncModeButtons();
    }

    void syncModeButtons()
    {
        const bool analyze = mProcessor->getUiMode() == "analyze";
        modePerfButton.setToggleState (! analyze, juce::dontSendNotification);
        modeAnaButton.setToggleState (analyze, juce::dontSendNotification);
    }

    static void styleAlertWindow (AlertWindow& window)
    {
        window.setLookAndFeel (new CustomLookAndFeel());
        window.setColour (AlertWindow::backgroundColourId, BorealColors::background);
        window.setColour (AlertWindow::textColourId, BorealColors::menuText);
        window.setColour (AlertWindow::outlineColourId, BorealColors::menuBorder);
    }

    static void raiseAlertButtons (AlertWindow& window)
    {
        for (int i = 0; i < window.getNumButtons(); ++i)
            if (auto* b = window.getButton (i))
            {
                b->setTopLeftPosition (b->getX(), b->getY() - 10);
                b->setColour (juce::TextButton::buttonColourId, BorealColors::buttonOn);
                b->setColour (juce::TextButton::buttonOnColourId, BorealColors::buttonBorder);
                b->setColour (juce::TextButton::textColourOffId, BorealColors::menuTextBright);
                b->setColour (juce::TextButton::textColourOnId, BorealColors::menuTextBright);
            }
    }

    void buttonClicked (Button* button) override
    {
        if (button == &mMenuButton)
            showHamburgerMenu();
        else if (button == &mPresetDisplay)
            showPresetMenu();
    }

    void addPresetLevel (PopupMenu& menu, const File& dir)
    {
        const int numPresets = mPresetManager->getNumberOfPresets();

        for (int i = 0; i < numPresets; ++i)
        {
            const File f = mPresetManager->getPresetFile (i);
            if (f.getParentDirectory() == dir)
                menu.addItem (i + 1, f.getFileNameWithoutExtension());
        }

        Array<File> subdirs;
        for (int i = 0; i < numPresets; ++i)
        {
            const File f = mPresetManager->getPresetFile (i);
            if (f == File() || f.getParentDirectory() == dir || ! f.isAChildOf (dir))
                continue;

            File ancestor = f.getParentDirectory();
            while (ancestor.getParentDirectory() != dir)
                ancestor = ancestor.getParentDirectory();

            if (! subdirs.contains (ancestor))
                subdirs.add (ancestor);
        }

        for (auto& sub : subdirs)
        {
            PopupMenu subMenu;
            addPresetLevel (subMenu, sub);
            if (subMenu.getNumItems() > 0)
                menu.addSubMenu (">> " + sub.getFileName(), subMenu);
        }
    }

    struct PresetSearchBox : public Component,
                             private juce::TextEditor::Listener,
                             private juce::ListBoxModel
    {
        PresetSearchBox (BorealPresetManager* mgr, std::function<void (int)> onPick)
        :   manager (mgr), pick (std::move (onPick))
        {
            search.setTextToShowWhenEmpty ("Search presets...", BorealColors::menuTextDim);
            search.setFont (CustomLookAndFeel::makeFont (24.0f));
            search.setIndents (juce::roundToInt (8.0f * BorealZoom::uiScale),
                               juce::roundToInt (10.0f * BorealZoom::uiScale));
            search.setColour (juce::TextEditor::backgroundColourId, BorealColors::menuBg);
            search.setColour (juce::TextEditor::textColourId, BorealColors::menuTextBright);
            search.setColour (juce::TextEditor::outlineColourId, BorealColors::menuBorder);
            search.addListener (this);
            addAndMakeVisible (search);

            list.setModel (this);
            list.setRowHeight (juce::roundToInt (36.0f * BorealZoom::uiScale));
            list.setColour (juce::ListBox::backgroundColourId, BorealColors::menuBg);
            list.setColour (juce::ListBox::textColourId, BorealColors::menuText);
            list.setColour (juce::ListBox::outlineColourId, BorealColors::menuBorder);
            addAndMakeVisible (list);

            refilter();
        }

        void resized() override
        {
            const int searchH = juce::roundToInt (44.0f * BorealZoom::uiScale);
            auto area = getLocalBounds();
            search.setBounds (area.removeFromTop (searchH));
            list.setBounds (area.withTrimmedTop (juce::roundToInt (6.0f * BorealZoom::uiScale)));
        }

        void visibilityChanged() override
        {
            if (isShowing())
                search.grabKeyboardFocus();
        }

        int getNumRows() override { return filtered.size(); }

        void paintListBoxItem (int row, Graphics& g, int w, int h, bool selected) override
        {
            if (selected)
            {
                g.setColour (BorealColors::menuHover);
                g.fillAll();
            }
            g.setColour (selected ? BorealColors::menuTextBright : BorealColors::menuText);
            g.setFont (CustomLookAndFeel::makeFont (20.0f));
            const int idx = filtered[(size_t) row];
            g.drawText (manager->getPresetFile (idx).getFileNameWithoutExtension(),
                        10, 0, w - 20, h, juce::Justification::centredLeft, true);
        }

        void listBoxItemClicked (int row, const juce::MouseEvent&) override
        {
            if (juce::isPositiveAndBelow (row, filtered.size()))
                pick (filtered[(size_t) row]);
        }

        void textEditorTextChanged (juce::TextEditor&) override { refilter(); }

        void textEditorEscapeKeyPressed (juce::TextEditor&) override
        {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState (0);
        }

        void textEditorReturnKeyPressed (juce::TextEditor&) override
        {
            if (filtered.size() == 1)
                pick (filtered[0]);
            else if (list.getSelectedRow() >= 0)
                pick (filtered[(size_t) list.getSelectedRow()]);
        }

    private:
        void refilter()
        {
            filtered.clear();
            const auto needle = search.getText().trim().toLowerCase();
            for (int i = 0, n = manager->getNumberOfPresets(); i < n; ++i)
                if (needle.isEmpty() || manager->getPresetFile (i).getFileNameWithoutExtension()
                        .toLowerCase().contains (needle))
                    filtered.add (i);
            list.updateContent();
            if (filtered.size() > 0)
                list.selectRow (0);
            repaint();
        }

        BorealPresetManager* manager;
        std::function<void (int)> pick;
        juce::TextEditor search;
        juce::ListBox list;
        juce::Array<int> filtered;
    };

    void showPresetSearch()
    {
        auto* box = new PresetSearchBox (mPresetManager, [this] (int index)
        {
            if (mPresetManager->loadPreset (index))
                currentPresetIndex = index;
            updatePresetDisplay();
        });
        box->setSize (juce::roundToInt (420.0f * BorealZoom::uiScale),
                      juce::roundToInt (360.0f * BorealZoom::uiScale));
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned (box);
        options.dialogTitle = "Search Presets";
        options.dialogBackgroundColour = BorealColors::background;
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = false;
        options.resizable = false;
        options.launchAsync();
    }

    void showPresetMenu()
    {
        PopupMenu menu;

        menu.addItem (kSearchPresetsId, "Search Presets...");
        menu.addSeparator();
        if (mPresetManager->getNumberOfPresets() == 0)
            menu.addItem (1, "(no presets found)", false);
        else
            addPresetLevel (menu, File (mPresetManager->getPresetDirectory()));
        menu.addSeparator();
        menu.addItem (kLoadFromFileId, "Load From File...");

        menu.showMenuAsync (
            PopupMenu::Options()
                .withTargetComponent (&mPresetDisplay),
            [this] (int result)
            {
                if (result == kSearchPresetsId)
                {
                    showPresetSearch();
                }
                else if (result == kLoadFromFileId)
                {
                    loadPresetFileDialog();
                }
                else if (result > 0)
                {
                    const int index = result - 1;
                    if (mPresetManager->loadPreset (index))
                        currentPresetIndex = index;
                    updatePresetDisplay();
                }
            });
    }

    void loadPresetFileDialog()
    {
        auto chooser = std::make_shared<FileChooser> ("Load Preset File",
                                                      File (mPresetManager->getPresetDirectory()),
                                                      "*" + String (PRESET_FILE_EXTENSION));

        chooser->launchAsync (FileBrowserComponent::openMode
                              | FileBrowserComponent::canSelectFiles,
                              [this, chooser] (const FileChooser& fc)
                              {
                                  if (fc.getResult().existsAsFile())
                                  {
                                      if (mPresetManager->loadPresetFile (fc.getResult()))
                                          currentPresetIndex = -1;
                                      updatePresetDisplay();
                                  }
                              });
    }

    void showHamburgerMenu()
    {
        PopupMenu menu;

        menu.addItem (MenuOption::Init, "Init");
        menu.addSeparator();
        menu.addItem (MenuOption::Save, "Save");
        menu.addItem (MenuOption::SaveAs, "Save As...");
        menu.addItem (MenuOption::LoadFromFile, "Load From File...");
        menu.addSeparator();
        menu.addItem (MenuOption::SetPresetFolder, "Set Preset Folder");
        menu.addItem (MenuOption::ResetPresetFolder, "Reset Preset Folder");
        menu.addSeparator();
        PopupMenu crtSub;
        crtSub.addItem (MenuOption::CrtEnabled,
                        juce::String ("CRT Enabled - ") + (mProcessor->isCrtEnabled() ? "[X]" : "[ ]"));

        PopupMenu strengthSub;
        const int strength = mProcessor->getCrtStrength();
        strengthSub.addItem (kCrtStrengthLow, "Low", true, strength == 0);
        strengthSub.addItem (kCrtStrengthMedium, "Medium", true, strength == 1);
        strengthSub.addItem (kCrtStrengthHigh, "High", true, strength == 2);
        crtSub.addSubMenu (">> Strength", strengthSub);
        menu.addSubMenu (">> CRT Layout", crtSub);

        PopupMenu zoomSub;
        const int zoomCount = (int) Boreal::Zoom::ZOOM_PERCENTS.size();
        int zoomCurrent = 0;
        if (auto* zoomParam = mProcessor->apvts.getParameter (Boreal::Zoom::UI_SCALE_ID))
            zoomCurrent = juce::jlimit (0, zoomCount - 1,
                                        juce::roundToInt (zoomParam->getValue() * (float) (zoomCount - 1)));
        for (int i = 0; i < zoomCount; ++i)
            zoomSub.addItem (kZoomBaseId + i,
                             juce::String (Boreal::Zoom::ZOOM_PERCENTS[(size_t) i], 0) + "%",
                             true, i == zoomCurrent);
        menu.addSubMenu (">> Zoom", zoomSub);

        menu.addSeparator();
        menu.addItem (MenuOption::About, "About");

        if (isInStandaloneApp (this))
        {
            menu.addSeparator();
            PopupMenu standaloneSub;
            standaloneSub.addItem (MenuOption::StandaloneAudioSettings, "Audio/MIDI Settings...");
            standaloneSub.addItem (MenuOption::StandaloneSaveState, "Save State...");
            standaloneSub.addItem (MenuOption::StandaloneLoadState, "Load State...");
            standaloneSub.addItem (MenuOption::StandaloneReset, "Reset to Default");
            menu.addSubMenu (">> Standalone", standaloneSub);
        }

        menu.showMenuAsync (
            PopupMenu::Options()
                .withTargetComponent (&mMenuButton),
            [this] (int result) { handleMenuResult (result); });
    }

    void handleMenuResult (int selected_id)
    {
        const int zoomCount = (int) Boreal::Zoom::ZOOM_PERCENTS.size();
        if (selected_id >= kZoomBaseId && selected_id < kZoomBaseId + zoomCount)
        {
            const int idx = selected_id - kZoomBaseId;
            if (auto* zoomParam = mProcessor->apvts.getParameter (Boreal::Zoom::UI_SCALE_ID))
                zoomParam->setValueNotifyingHost ((float) idx / (float) (zoomCount - 1));
            return;
        }

        switch (selected_id)
        {
            case MenuOption::None:                                 break;
            case MenuOption::Init:   displayInitPopup();           break;
            case MenuOption::Save:   mPresetManager->savePreset(); break;
            case MenuOption::SaveAs: displaySaveAsPopup();         break;
            case MenuOption::LoadFromFile: loadPresetFileDialog(); break;
            case MenuOption::SetPresetFolder: displaySetPresetFolderPopup(); break;
            case MenuOption::ResetPresetFolder: resetPresetFolder(); break;
            case MenuOption::About: displayAboutPopup(); break;
            case MenuOption::CrtEnabled:
                mProcessor->setCrtEnabled (! mProcessor->isCrtEnabled());
                break;
            case kCrtStrengthLow:
                mProcessor->setCrtStrength (0);
                break;
            case kCrtStrengthMedium:
                mProcessor->setCrtStrength (1);
                break;
            case kCrtStrengthHigh:
                mProcessor->setCrtStrength (2);
                break;
            case MenuOption::StandaloneAudioSettings:
                if (auto* tl = getTopLevelComponent())
                    if (auto* sfw = dynamic_cast<juce::BorealFilterWindow*> (tl))
                        new SettingsWindow (sfw->getPluginHolder()->deviceManager);
                break;
            case MenuOption::StandaloneSaveState:
                if (auto* tl = getTopLevelComponent())
                    if (auto* sfw = dynamic_cast<juce::BorealFilterWindow*> (tl))
                        sfw->getPluginHolder()->askUserToSaveState();
                break;
            case MenuOption::StandaloneLoadState:
                if (auto* tl = getTopLevelComponent())
                    if (auto* sfw = dynamic_cast<juce::BorealFilterWindow*> (tl))
                        sfw->getPluginHolder()->askUserToLoadState();
                break;
            case MenuOption::StandaloneReset:
                if (auto* tl = getTopLevelComponent())
                    if (auto* sfw = dynamic_cast<juce::BorealFilterWindow*> (tl))
                        sfw->resetToDefaultState();
                break;
            default:                 jassertfalse;                 break;
        }
    }

    void displayInitPopup()
    {
        auto* dialog = new BorealCardDialog (">> INIT",
                                             "Are you sure you want to initialize this preset?",
                                             "CONFIRM", "CANCEL");
        juce::Component::SafePointer<PresetManagerPanel> safeThis { this };
        dialog->onConfirm = [safeThis]
        {
            if (safeThis == nullptr)
                return;
            safeThis->mPresetManager->createNewPreset();
            safeThis->mProcessor->resetAnalyzerDefaults();
            safeThis->mProcessor->setUiMode ("performance");
            if (auto* tl = safeThis->getTopLevelComponent())
            {
                std::function<void (juce::Component*)> resetViews = [&] (juce::Component* c)
                {
                    if (c == nullptr)
                        return;
                    if (auto* mp = dynamic_cast<MorphingPanel*> (c))
                        mp->selectSection (0);
                    if (auto* sp = dynamic_cast<SoundPanel*> (c))
                        sp->setCursorView (false);
                    for (auto* child : c->getChildren())
                        resetViews (child);
                };
                resetViews (tl);
            }
            safeThis->updatePresetDisplay();
        };
        BorealDialogs::openWindow (dialog, "Init", this,
                                    juce::roundToInt (460.0f * BorealZoom::uiScale),
                                    juce::roundToInt (220.0f * BorealZoom::uiScale));
    }

    void displaySaveAsPopup()
    {
        String currentPresetName = mPresetManager->getCurrentPresetName();

        juce::Component::SafePointer<PresetManagerPanel> safeThis { this };
        auto* dialog = new BorealSaveAsDialog (currentPresetName, [safeThis] (const String& presetName)
        {
            if (safeThis == nullptr || presetName.trim().isEmpty())
                return;
            safeThis->mPresetManager->saveAsPreset (presetName);
            safeThis->updatePresetDisplay();
        });
        BorealDialogs::openWindow (dialog, "Save As", this,
                                   juce::roundToInt (460.0f * BorealZoom::uiScale),
                                   juce::roundToInt (220.0f * BorealZoom::uiScale));
    }

    void displaySetPresetFolderPopup()
    {
        FileChooser chooser ("Select Preset Folder",
                             File (mPresetManager->getPresetDirectory()),
                             "*",
                             true,
                             false,
                             this);

        if (chooser.browseForDirectory())
        {
            if (mPresetManager->setPresetDirectory (chooser.getResult().getFullPathName()))
            {
                currentPresetIndex = -1;
                updatePresetDisplay();
            }
        }
    }

    void resetPresetFolder()
    {
        mPresetManager->resetPresetDirectoryToDefault();
        currentPresetIndex = -1;
        updatePresetDisplay();
    }

    void displayAboutPopup()
    {

        new AboutWindow();
    }

    void updatePresetDisplay()
    {
        String presetName = mPresetManager->getCurrentPresetName();
        mPresetDisplay.setButtonText (presetName);
    }

    BorealPresetManager* mPresetManager;

    TextButton mPresetDisplay;
    PerfModeButton modePerfButton;
    AnaModeButton modeAnaButton;
    HamburgerButton mMenuButton;

    int currentPresetIndex = 0;

    BorealAudioProcessor* mProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManagerPanel)
};
