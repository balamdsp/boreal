#pragma once

#include "MainPanel.h"
#include "TopPanel.h"
#include "AnalyzerWindow.h"

#include "../Components/CRTScreen.h"
#include "../Helpers/CustomLookAndFeel.h"
#include "../Helpers/InterfaceDefines.h"
#include "../Helpers/BorealConstants.h"

struct AnchoredTooltipWindow : public juce::TooltipWindow
{
    void setTipsEnabled (bool enabled) noexcept
    {
        if (tipsEnabled != enabled)
        {
            tipsEnabled = enabled;
            if (! tipsEnabled)
                hideTip();
            else
                repaint();
        }
    }

    juce::String getTipFor (juce::Component& c) override
    {
        if (! tipsEnabled)
            return {};
        if (auto* parent = getParentComponent())
        {
            const auto r = parent->getLocalArea (&c, c.getLocalBounds());
            CustomLookAndFeel::TooltipAnchor::pos = r.getCentre();
            CustomLookAndFeel::TooltipAnchor::valid = true;
        }
        return juce::TooltipWindow::getTipFor (c);
    }

private:
    bool tipsEnabled = true;
};

class BorealPanel : public Component,
                    public DragAndDropContainer
{
public:
    BorealPanel (BorealAudioProcessor* inProcessor)
        : mProcessor (inProcessor),
          screen (inProcessor),
          crtOverlay (&screen, &inProcessor->getCrtEnabledFlag())
    {
        static CustomLookAndFeel customLookAndFeel;
        setLookAndFeel (&customLookAndFeel);
        juce::LookAndFeel::setDefaultLookAndFeel (&customLookAndFeel);

        addAndMakeVisible (screen);
        addAndMakeVisible (crtOverlay);
        sendLookAndFeelChange();
        crtOverlay.setStrengthSource (&inProcessor->getCrtStrengthFlag());
        crtOverlay.setCrtStrength (inProcessor->getCrtStrength());
        crtOverlay.toBehind (&screen);
        crtOverlay.toFront (false);

        setSize (juce::roundToInt ((float) BOREAL_PANEL_WIDTH * BorealZoom::uiScale),
                 juce::roundToInt ((float) BOREAL_PANEL_HEIGHT * BorealZoom::uiScale));
    }

    ~BorealPanel() override {}

    void paint (Graphics& g) override
    {
        g.fillAll (GUI::Color::Background);
    }

    void resized() override
    {

        crtOverlay.setBounds (getLocalBounds());
        screen.setBounds (getLocalBounds());

        const float pad = CRTScreen::getFrameSize();
        const float scale = 1.0f / (1.0f + 2.0f * pad);
        const float tx = getWidth()  * 0.5f * (1.0f - scale);
        const float ty = getHeight() * 0.5f * (1.0f - scale);

        screen.setTransform (mProcessor != nullptr && mProcessor->isCrtEnabled()
                                 ? juce::AffineTransform::scale (scale, scale).translated (tx, ty)
                                 : juce::AffineTransform());
    }

private:

    struct Screen : public Component, private juce::Value::Listener
    {
        Screen (BorealAudioProcessor* inProcessor)
            : topPanel (inProcessor),
              mainPanel (inProcessor),
              analyzerWindow (inProcessor),
              mProcessor (inProcessor)
        {
            addAndMakeVisible (topPanel);
            addAndMakeVisible (mainPanel);
            addAndMakeVisible (analyzerWindow);
            analyzerWindow.setVisible (false);
            addChildComponent (tooltipWindow);

            mProcessor->uiModeValue.addListener (this);
            mProcessor->tooltipDelayMsValue.addListener (this);
            mProcessor->tooltipsEnabledValue.addListener (this);
            applyUiMode();
            applyTooltipSettings();
        }

        ~Screen() override
        {
            if (mProcessor != nullptr)
            {
                mProcessor->uiModeValue.removeListener (this);
                mProcessor->tooltipDelayMsValue.removeListener (this);
                mProcessor->tooltipsEnabledValue.removeListener (this);
            }
        }

        void paint (Graphics&) override {}

        void resized() override
        {
            FlexBox fb;
            fb.flexDirection = FlexBox::Direction::column;

            float topPanelHeight = getHeight() / 8.0f;

            FlexItem top (static_cast<float> (getWidth()), topPanelHeight, topPanel);

            auto contentBounds = getLocalBounds().withTrimmedTop ((int) topPanelHeight);
            mainPanel.setBounds (contentBounds);
            analyzerWindow.setBounds (contentBounds);

            fb.items.add (top);
            fb.performLayout (getLocalBounds().toFloat());
        }

        void valueChanged (juce::Value& value) override
        {
            if (value.refersToSameSourceAs (mProcessor->uiModeValue))
                applyUiMode();
            else
                applyTooltipSettings();
        }

    private:
        void applyUiMode()
        {
            const bool analyze = mProcessor != nullptr && mProcessor->getUiMode() == "analyze";
            mainPanel.setVisible (! analyze);
            analyzerWindow.setVisible (analyze);
        }

        void applyTooltipSettings()
        {
            if (mProcessor == nullptr)
                return;
            tooltipWindow.setMillisecondsBeforeTipAppears (
                juce::jmax (0, mProcessor->getTooltipDelayMs()));
            tooltipWindow.setTipsEnabled (mProcessor->getTooltipsEnabled());
        }

    public:
        TopPanel topPanel;
        MainPanel mainPanel;
        AnalyzerWindow analyzerWindow;

    private:
        BorealAudioProcessor* mProcessor = nullptr;
        AnchoredTooltipWindow tooltipWindow;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Screen)
    };

    BorealAudioProcessor* mProcessor = nullptr;
    Screen screen;
    CRTScreen crtOverlay { &screen, nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BorealPanel)
};
