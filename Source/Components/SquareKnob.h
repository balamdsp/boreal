#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <functional>
#include <memory>

#include "../Helpers/CustomLookAndFeel.h"
#include "../Helpers/InterfaceDefines.h"
#include "../Helpers/BorealConstants.h"
#include "../Helpers/BorealParameters.h"

class SquareKnob : public juce::Slider,
                   private juce::Slider::Listener
{
public:
    using Formatter = std::function<juce::String (double)>;

    SquareKnob (juce::Value source, const juce::String& name,
                double lo, double hi, double step,
                const juce::String& unitSuffix = {}, double skewFactor = 1.0,
                Formatter formatter = {})
    :   juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag,
                      juce::Slider::NoTextBox),
        knobName (name), unit (unitSuffix),
        integral (step >= 1.0),
        valueFormatter (std::move (formatter)),
        boundValue (source), valueBound (true)
    {
        initCommon (lo, hi, step, skewFactor);
        setDoubleClickReturnValue (true, (double) boundValue.getValue());
        refresh();
        addListener (this);
    }

    SquareKnob (juce::AudioProcessorValueTreeState& apvts,
                Boreal::Parameters param,
                const juce::String& name,
                Formatter formatter = {},
                double skewFactor = 1.0)
    :   juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag,
                      juce::Slider::NoTextBox),
        knobName (name),
        integral (false),
        valueFormatter (std::move (formatter))
    {
        const Boreal::Parameter<float>& meta = Boreal::PARAMETERS<float>[param];
        initCommon ((double) meta.min_value, (double) meta.max_value, 0.001, skewFactor);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, meta.ID, *this);
        setDoubleClickReturnValue (true, (double) meta.default_value);
        addListener (this);
    }

    void paint (juce::Graphics& g) override
    {
        const float zs = BorealZoom::uiScale;
        auto bounds = getLocalBounds().toFloat();

        const float labelH = 17.0f * zs;
        auto body = bounds;
        auto labelArea = body.removeFromBottom (labelH);
        const float side = juce::jmin (body.getWidth(), body.getHeight());
        if (side <= 0.0f)
            return;
        auto sq = juce::Rectangle<float> (body.getCentreX() - side * 0.5f,
                                          body.getCentreY() - side * 0.5f,
                                          side, side).reduced (2.0f * zs);
        if (squareYOffsetU != 0.0f)
            sq.translate (0.0f, juce::jmin (squareYOffsetU * zs,
                                            juce::jmax (0.0f, body.getBottom() - sq.getBottom())));
        const float corner = 3.0f * zs;

        g.setColour (GUI::Color::Background);
        g.fillRoundedRectangle (sq, corner);

        const double proportion
            = juce::jlimit (0.0, 1.0, valueToProportionOfLength (getValue()));
        if (proportion > 0.0)
        {
            const float fillH = (float) proportion * sq.getHeight();
            const auto fill = sq.withTrimmedTop (sq.getHeight() - fillH);
            g.saveState();
            g.reduceClipRegion (juce::Rectangle<int> (sq.toNearestInt()));
            g.setColour (GUI::Color::Logo.withAlpha (
                isMouseOverOrDragging() ? 0.38f : 0.28f));
            g.fillRect (fill);
            g.restoreState();
        }

        g.setColour (GUI::Color::Logo.withAlpha (0.60f));
        g.drawRoundedRectangle (sq, corner, 1.0f * zs);

        g.setColour (GUI::Color::KeyDown);
        {
            const juce::String text = formatValue (getValue());
            const float availW = sq.getWidth() - 4.0f * zs;
            float valueSize = 18.0f * fontScale;
            juce::Font f = CustomLookAndFeel::makeFont (valueSize);
            while (valueSize > 12.5f
                   && juce::GlyphArrangement::getStringWidthInt (f, text) > juce::roundToInt (availW))
            {
                valueSize -= 0.5f;
                f = CustomLookAndFeel::makeFont (valueSize);
            }
            g.setFont (f);
            g.drawText (text, sq, juce::Justification::centred, false);
        }

        g.setColour (GUI::Color::Logo.withAlpha (0.75f));
        g.setFont (CustomLookAndFeel::makeFont (14.0f * fontScale));
        g.drawText (knobName, labelArea.translated (0.0f, 2.0f * zs),
                    juce::Justification::centred, false);
    }

    void refresh()
    {
        if (valueBound)
            setValue ((double) boundValue.getValue(),
                      juce::NotificationType::dontSendNotification);
    }

    void setSquareYOffsetU (float units) noexcept
    {
        if (squareYOffsetU != units)
        {
            squareYOffsetU = units;
            repaint();
        }
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
    void initCommon (double lo, double hi, double step, double skewFactor)
    {
        setRange (lo, hi, step);
        if (! juce::approximatelyEqual (skewFactor, 1.0))
            setSkewFactor (skewFactor);
        setScrollWheelEnabled (true);
    }

    juce::String formatValue (double v) const
    {
        if (valueFormatter)
            return valueFormatter (v);
        return (integral ? juce::String ((int) std::round (v))
                         : juce::String (v, 2)) + unit;
    }

    void sliderValueChanged (juce::Slider*) override
    {
        if (valueBound)
            boundValue.setValue (getValue());
        if (onValueChangeInternal)
            onValueChangeInternal();
    }

    void valueChanged() override { repaint(); }

    juce::String knobName;
    juce::String unit;
    bool integral = false;
    float squareYOffsetU = 0.0f;
    float fontScale = 1.0f;
    Formatter valueFormatter;
    juce::Value boundValue;
    bool valueBound = false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SquareKnob)
};
