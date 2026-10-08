#pragma once

#include <JuceHeader.h>

#include "../Helpers/CustomLookAndFeel.h"
#include "../Helpers/InterfaceDefines.h"
#include "../Helpers/BorealConstants.h"

#include "../Components/Slider.h"

#include "PresetManagerPanel.h"
#include "../PluginProcessor.h"

class TopPanel : public Component
{
public:

    TopPanel (BorealAudioProcessor* inProcessor)
    :   centerPanel (inProcessor),
        rightPanel (inProcessor)
    {
        addAndMakeVisible (leftPanel);
        addAndMakeVisible (centerPanel);
        addAndMakeVisible (rightPanel);
        addAndMakeVisible (brandPanel);
    }

    ~TopPanel() {}

    void paint (Graphics&) override {}

    void resized() override
    {
        FlexBox fb;

        FlexItem left = FlexItem (leftPanel).withFlex (40.0f);
        FlexItem center = FlexItem (centerPanel).withFlex (73.0f);
        FlexItem spacer = FlexItem().withFlex (5.0f);
        FlexItem right = FlexItem (rightPanel).withFlex (52.0f);
        FlexItem brand = FlexItem (brandPanel).withFlex (20.0f);

        fb.items.addArray ({left, center, spacer, right, brand});
        fb.performLayout (getLocalBounds().toFloat());
    }

private:

    struct LeftSidePanel : public Component, private Timer
    {
        LeftSidePanel () { startTimerHz (2); }

        void paint (Graphics& g) override
        {

            const float s = BorealZoom::uiScale;
            const int margin = juce::roundToInt (18.0f * s);
            const int text_width = getWidth() - margin * 2;

            const float line_gap = 4.0f * s;
            const float line1_height = 30.0f * s;
            const float line2_height = 17.0f * s;
            const float block_height = line1_height + line_gap + line2_height;
            const float block_y = (getHeight() - block_height) / 2.0f;

            const String banner = "BOREAL...";
            const auto bannerFont = CustomLookAndFeel::makeFont (40.0f);
            g.setFont (bannerFont);
            g.setColour (GUI::Color::Logo);
            g.drawText (banner, margin, (int) block_y, text_width, (int) line1_height,
                        Justification::centredLeft, false);

            g.setFont (CustomLookAndFeel::makeFont (17.0f));
            g.setColour (GUI::Color::Logo.withAlpha (0.70f));
            g.drawText ("SPECTRAL MORPHING SYNTH",
                        margin, (int) (block_y + line1_height + line_gap), text_width,
                        (int) line2_height, Justification::centredLeft, false);

            if (cursorVisible)
            {
                const float bannerWidth = (float) juce::GlyphArrangement::getStringWidthInt (bannerFont, banner);
                const float asc = bannerFont.getAscent();
                const float desc = bannerFont.getDescent();
                const float baseline = block_y + (line1_height - (asc + desc)) * 0.5f + asc;
                const float curTop = baseline - asc * 0.7f - 2.0f * s;
                const float curBottom = baseline + 1.0f * s;
                g.setColour (GUI::Color::Logo.withAlpha (0.85f));
                g.fillRect ((float) margin + bannerWidth + 8.0f * s, curTop,
                            13.0f * s, curBottom - curTop);
            }
        }

        void timerCallback() override
        {
            cursorVisible = ! cursorVisible;
            repaint();
        }

        bool cursorVisible = true;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LeftSidePanel)
    };

    struct CenterPanel : public Component
    {
        CenterPanel (BorealAudioProcessor* inProcessor)
        :   preset_manager_panel (inProcessor)
        {
            addAndMakeVisible (preset_manager_panel);
        }

        void paint (Graphics&) override {}

        void resized() override
        {
            auto area = getLocalBounds();
            preset_manager_panel.setBounds (area);
        }

        PresetManagerPanel preset_manager_panel;
    };

    struct RightSidePanel : public Component
    {
        RightSidePanel (BorealAudioProcessor* inProcessor)
        {
            output_gain_slider = new Boreal::Slider (inProcessor->apvts,
                                                     Boreal::Parameters::OutputGain,
                                                     Slider::LinearHorizontal);
            output_gain_slider->setTooltip ("Master output gain -- final level after the safety limiter");

            addAndMakeVisible (output_gain_slider);
        }

        void paint (Graphics&) override {}

        void resized() override
        {
            auto bounds = getLocalBounds().reduced (juce::roundToInt (6.0f * BorealZoom::uiScale), 0);
            output_gain_slider->setBounds (bounds.withSizeKeepingCentre (juce::roundToInt (bounds.getWidth() * 0.8f),
                                                juce::roundToInt (28.0f * BorealZoom::uiScale)));
        }

        Boreal::Slider* output_gain_slider;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RightSidePanel)
    };

    struct BrandPanel : public Component
    {
        void paint (Graphics& g) override
        {
            const float s = BorealZoom::uiScale;
            const int margin = juce::roundToInt (18.0f * s);

            const float brandH = 26.0f * s;
            const float gap = -4.0f * s;
            const float versionH = 24.0f * s;
            const float blockH = brandH + gap + versionH;
            const float blockY = (getHeight() - blockH) / 2.0f;
            const int textW = getWidth() - margin;

            g.setColour (GUI::Color::Logo);
            g.setFont (CustomLookAndFeel::makeFont (26.0f));
            g.drawText ("BalamDSP", 0, (int) blockY, textW, (int) brandH,
                        Justification::centredRight, false);

            g.setColour (GUI::Color::Logo.withAlpha (0.55f));
            g.setFont (CustomLookAndFeel::makeFont (24.0f));
            g.drawText ("v" + String (PLUGIN_VERSION), 0,
                        (int) (blockY + brandH + gap), textW, (int) versionH,
                        Justification::centredRight, false);
        }
    };

    LeftSidePanel leftPanel;
    CenterPanel centerPanel;
    RightSidePanel rightPanel;
    BrandPanel brandPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopPanel)
};
