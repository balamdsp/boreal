#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>

#include "../Helpers/CustomLookAndFeel.h"
#include "../Helpers/InterfaceDefines.h"
#include "../Helpers/BorealConstants.h"
#include "../Helpers/BorealParameters.h"

class BipolarFader : public juce::Slider,
                     private juce::Slider::Listener
{
public:
    explicit BipolarFader (juce::Value v)
    :   juce::Slider (juce::Slider::LinearHorizontal,
                      juce::Slider::NoTextBox),
        bound (v)
    {
        setRange (-100.0, 100.0, 1.0);
        setValue ((double) bound.getValue(),
                  juce::NotificationType::dontSendNotification);
        setScrollWheelEnabled (true);
        setDoubleClickReturnValue (true, 0.0);
        setSliderSnapsToMousePosition (false);
        addListener (this);
    }

    BipolarFader (AudioProcessorValueTreeState& state_to_control,
                  const Boreal::Parameters parameter_num,
                  double step = 1.0)
    :   juce::Slider (juce::Slider::LinearHorizontal,
                      juce::Slider::NoTextBox)
    {
        const Boreal::Parameter<float>& meta = Boreal::PARAMETERS<float>[parameter_num];
        setRange ((double) meta.min_value, (double) meta.max_value, step);
        setScrollWheelEnabled (true);
        setDoubleClickReturnValue (true, (double) meta.default_value);
        setSliderSnapsToMousePosition (false);

        mAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (
            state_to_control, meta.ID, *this);
        addListener (this);
    }

    void paint (juce::Graphics& g) override
    {
        const float zs = BorealZoom::uiScale;
        auto track = getLocalBounds().toFloat().reduced (2.0f * zs);
        if (track.getWidth() <= 0.0f || track.getHeight() <= 0.0f)
            return;
        const float corner = 3.0f * zs;

        g.setColour (GUI::Color::Background);
        g.fillRoundedRectangle (track, corner);

        const double proportion
            = juce::jlimit (0.0, 1.0, valueToProportionOfLength (getValue()));
        const float cx = track.getCentreX();
        const float edge = track.getX() + (float) proportion * track.getWidth();
        const auto fill = edge >= cx
            ? juce::Rectangle<float> (cx, track.getY(), edge - cx, track.getHeight())
            : juce::Rectangle<float> (edge, track.getY(), cx - edge, track.getHeight());
        if (! fill.isEmpty())
        {
            g.saveState();
            g.reduceClipRegion (juce::Rectangle<int> (track.toNearestInt()));
            g.setColour (GUI::Color::Logo.withAlpha (
                isMouseOverOrDragging() ? 0.38f : 0.28f));
            g.fillRect (fill);
            g.restoreState();
        }

        g.setColour (GUI::Color::Logo.withAlpha (0.60f));
        g.drawRoundedRectangle (track, corner, 1.0f * zs);
        g.fillRect (cx - 0.5f * zs, track.getY() + 2.0f * zs,
                    1.0f * zs, track.getHeight() - 4.0f * zs);

        g.setColour (GUI::Color::KeyDown);
        g.setFont (CustomLookAndFeel::makeFont (14.0f * fontScale));
        const juce::String text = juce::String (juce::roundToInt (getValue())) + "%";
        g.drawText (text, track, juce::Justification::centred, false);
    }

    void refresh()
    {
        setValue ((double) bound.getValue(),
                  juce::NotificationType::dontSendNotification);
    }

    void setFontScale (float s) noexcept
    {
        if (fontScale != s)
        {
            fontScale = s;
            repaint();
        }
    }

    std::function<void()> onValueChangeInternal;

private:
    void sliderValueChanged (juce::Slider*) override
    {
        bound.setValue (getValue());
        if (onValueChangeInternal)
            onValueChangeInternal();
    }

    juce::Value bound;
    float fontScale = 1.0f;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> mAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BipolarFader)
};
