#pragma once

#include <JuceHeader.h>
#include <memory>

#include "SoundPanel.h"
#include "CorePanel.h"
#include "SettingsPanel.h"

#include "../Components/CollectionBrowser.h"
#include "../Components/Slider.h"
#include "../Helpers/InterfaceDefines.h"
#include "../Helpers/BorealConstants.h"
#include "../PluginProcessor.h"

class MorphingPanel :   public Component
{
public:
    MorphingPanel (BorealAudioProcessor* inProcessor)
    :   leftPanel (inProcessor),
        centerPanel (inProcessor)
    {
        addAndMakeVisible (leftPanel);
        addAndMakeVisible (centerPanel);

        coreButton.setButtonText ("CORE");
        coreButton.setTooltip ("Core section -- morph pad, playback, envelope and voice");
        soundsButton.setButtonText ("SOUNDS");
        soundsButton.setTooltip ("Sounds section -- sample library browser");
        settingsButton.setButtonText ("SETTINGS");
        settingsButton.setTooltip ("Settings section -- morph, transients and interface");

        coreButton.setClickingTogglesState (true);
        soundsButton.setClickingTogglesState (true);
        settingsButton.setClickingTogglesState (true);

        coreButton.setToggleState (true, NotificationType::dontSendNotification);

        coreButton.onClick     = [this] { selectSection (0); };
        soundsButton.onClick   = [this] { selectSection (1); };
        settingsButton.onClick = [this] { selectSection (2); };

        addAndMakeVisible (coreButton);
        addAndMakeVisible (soundsButton);
        addAndMakeVisible (settingsButton);

        selectSection (0);
    }

    ~MorphingPanel() {}

    void paint (Graphics&) override {}

    void resized() override
    {
        auto area = getLocalBounds().reduced (juce::roundToInt (GUI::Layout::MainMargin));
        const float zs = BorealZoom::uiScale;

        const float buttonWidth  = 130.0f * zs;
        const float buttonHeight = 30.0f * zs;
        const float buttonGap    = 8.0f * zs;

        FlexBox tabs;
        tabs.flexDirection = FlexBox::Direction::row;
        tabs.justifyContent = FlexBox::JustifyContent::center;

        tabs.items.add (FlexItem (buttonWidth, buttonHeight, coreButton).withMargin ({ 0.0f, buttonGap / 2.0f, 0.0f, buttonGap / 2.0f }));
        tabs.items.add (FlexItem (buttonWidth, buttonHeight, soundsButton).withMargin ({ 0.0f, buttonGap / 2.0f, 0.0f, buttonGap / 2.0f }));
        tabs.items.add (FlexItem (buttonWidth, buttonHeight, settingsButton).withMargin ({ 0.0f, buttonGap / 2.0f, 0.0f, buttonGap / 2.0f }));

        tabs.performLayout (Rectangle<int> (area.getX(), area.getY(), area.getWidth(), (int) buttonHeight));

        auto content = area.withTrimmedTop ((int) buttonHeight + juce::roundToInt (8.0f * zs));

        Grid grid;

        const float gap = GUI::Layout::MainGap;

        using Track = Grid::TrackInfo;

        grid.templateRows = { Track (Grid::Fr(1)) };
        grid.templateColumns = { Track (Grid::Fr(10)), Track (Grid::Fr(14)) };
        grid.columnGap = Grid::Px (gap);

        grid.items.add (GridItem (leftPanel));
        grid.items.add (GridItem (centerPanel));

        grid.performLayout (content);
    }

    void selectSection (int index)
    {
        coreButton.setToggleState (index == 0, NotificationType::dontSendNotification);
        soundsButton.setToggleState (index == 1, NotificationType::dontSendNotification);
        settingsButton.setToggleState (index == 2, NotificationType::dontSendNotification);

        centerPanel.setSection (index);
    }

private:

    struct LeftSidePanel : public Component, private juce::Timer
    {
        LeftSidePanel (BorealAudioProcessor* inProcessor)
        :   mProcessor (inProcessor),
            sound1Panel (inProcessor, 1),
            sound2Panel (inProcessor, 2),
            sound3Panel (inProcessor, 3),
            sound4Panel (inProcessor, 4)
        {
            addAndMakeVisible (sound1Panel);
            addAndMakeVisible (sound2Panel);
            addAndMakeVisible (sound3Panel);
            addAndMakeVisible (sound4Panel);
            startTimer (33);
        }

        void paint (Graphics& g) override
        {
            const float zs = BorealZoom::uiScale;
            const float barW = 5.0f * zs;
            const float barX = (float) getWidth() - 5.0f * zs - barW;

            for (int i = 0; i < 4; ++i)
            {
                const auto row = rowRects[(size_t) i];
                if (row.getHeight() <= 0.0f)
                    continue;
                const float barTop = row.getY() + juce::roundToInt (8.0f * zs);
                const float barBottom = row.getBottom() - juce::roundToInt (8.0f * zs);
                const Rectangle<float> vbar (barX, barTop, barW, barBottom - barTop);

                g.setColour (GUI::Color::Logo.withAlpha (0.20f));
                g.fillRect (vbar);

                const float pos = (mProcessor != nullptr)
                    ? mProcessor->getSlotPhaseDisplay (i) : -1.0f;
                if (pos >= 0.0f)
                {
                    g.setColour (GUI::Color::KeyDown);
                    const float fill = juce::jlimit (0.0f, 1.0f, pos);
                    const float fillH = juce::jmax (barW, vbar.getHeight() * fill);
                    g.fillRect (vbar.withTrimmedTop (vbar.getHeight() - fillH));
                }
            }
        }

        void timerCallback() override
        {
            bool changed = false;
            for (int i = 0; i < 4; ++i)
            {
                const float pos = (mProcessor != nullptr)
                    ? mProcessor->getSlotPhaseDisplay (i) : -1.0f;
                if (std::abs (pos - lastPos[(size_t) i]) > 0.0001f)
                {
                    lastPos[(size_t) i] = pos;
                    changed = true;
                }
            }
            if (changed)
                repaint();
        }

        void resized() override
        {
            FlexBox fb;
            fb.flexDirection = FlexBox::Direction::column;

            const float edge = GUI::Layout::CardInset * BorealZoom::uiScale;
            const float barSep = 6.0f * BorealZoom::uiScale;
            const float barPad = 5.0f * BorealZoom::uiScale;
            const float barW0 = 5.0f * BorealZoom::uiScale;
            const auto stack = getLocalBounds().toFloat().withTrimmedTop (edge).withTrimmedBottom (edge)
                                                          .withTrimmedLeft (edge)
                                                          .withTrimmedRight (barSep + barW0 + barPad);

            const float gap = GUI::Layout::CardGap;
            const float soundPanelHeight = (stack.getHeight() - gap * 3.0f) / 4.0f;

            FlexItem sound1 (stack.getWidth(), soundPanelHeight, sound1Panel);
            FlexItem sound2 (stack.getWidth(), soundPanelHeight, sound2Panel);
            FlexItem sound3 (stack.getWidth(), soundPanelHeight, sound3Panel);
            FlexItem sound4 (stack.getWidth(), soundPanelHeight, sound4Panel);

            fb.items.addArray
            ({
                sound1.withMargin (FlexItem::Margin (0.0f, 0.0f, gap / 2.0f, 0.0f)),
                sound2.withMargin (FlexItem::Margin (gap / 2.0f, 0.0f, gap / 2.0f, 0.0f)),
                sound3.withMargin (FlexItem::Margin (gap / 2.0f, 0.0f, gap / 2.0f, 0.0f)),
                sound4.withMargin (FlexItem::Margin (gap / 2.0f, 0.0f, 0.0f, 0.0f)),
            });

            fb.performLayout (stack);

            rowRects[0] = sound1Panel.getBounds().toFloat();
            rowRects[1] = sound2Panel.getBounds().toFloat();
            rowRects[2] = sound3Panel.getBounds().toFloat();
            rowRects[3] = sound4Panel.getBounds().toFloat();
        }

        SoundPanel sound1Panel;
        SoundPanel sound2Panel;
        SoundPanel sound3Panel;
        SoundPanel sound4Panel;

        BorealAudioProcessor* mProcessor = nullptr;
        juce::Rectangle<float> rowRects[4];
        float lastPos[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
    };

    struct CenterPanel : public Component
    {

        CenterPanel (BorealAudioProcessor* inProcessor)
        {
            mCorePanel = std::make_unique<CorePanel> (inProcessor);
            addAndMakeVisible (mCorePanel.get());

            mSettingsPanel = std::make_unique<SettingsPanel> (inProcessor);
            addAndMakeVisible (mSettingsPanel.get());

            mBrowser = std::make_unique<CollectionBrowser>();
            addAndMakeVisible (mBrowser.get());
            mBrowser->setVisible (false);
        }

        void paint (Graphics&) override {}

        void resized() override
        {
            const auto content = getLocalBounds();

            mCorePanel->setBounds (content);
            mSettingsPanel->setBounds (content);
            mBrowser->setBounds (content);
        }

        void setSection (int index)
        {
            mCorePanel->setVisible (index == 0);
            mBrowser->setVisible (index == 1);
            mSettingsPanel->setVisible (index == 2);
        }

        std::unique_ptr<CorePanel> mCorePanel;
        std::unique_ptr<SettingsPanel> mSettingsPanel;
        std::unique_ptr<CollectionBrowser> mBrowser;
    };

    LeftSidePanel leftPanel;
    CenterPanel centerPanel;

    TextButton coreButton, soundsButton, settingsButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MorphingPanel)
};
