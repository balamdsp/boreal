#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <functional>

#include "../Helpers/CustomLookAndFeel.h"
#include "../Helpers/InterfaceDefines.h"
#include "../Helpers/BorealConstants.h"
#include "../Helpers/BorealParameters.h"

class SquareFader : public juce::Slider, private juce::Slider::Listener
{
public:
    static constexpr float capWidthU = 16.0f;

    using Formatter = std::function<juce::String (double)>;

    SquareFader (juce::Value source, const juce::String& name,
                 double lo, double hi, double step,
                 const juce::String& unitSuffix = {}, double skewFactor = 1.0,
                 int valueDecimals = 1)
    :   juce::Slider (juce::Slider::LinearVertical,
                      juce::Slider::NoTextBox),
        faderName (name), unit (unitSuffix),
        boundValue (source), decimals (valueDecimals)
    {
        setRange (lo, hi, step);
        if (! juce::approximatelyEqual (skewFactor, 1.0))
            setSkewFactor (skewFactor);
        setScrollWheelEnabled (true);
        setLookAndFeel (&faderLnf);
        setDoubleClickReturnValue (true, (double) boundValue.getValue());
        refresh();
        addListener (this);
    }

    SquareFader (AudioProcessorValueTreeState& state_to_control,
                 const Boreal::Parameters parameter_num,
                 const juce::String& name,
                 const juce::String& unitSuffix = {}, double skewFactor = 1.0,
                 int valueDecimals = 1, double interval = 0.001,
                 Formatter formatter = {})
    :   juce::Slider (juce::Slider::LinearVertical,
                      juce::Slider::NoTextBox),
        faderName (name), unit (unitSuffix), decimals (valueDecimals),
        valueFormatter (std::move (formatter))
    {
        Boreal::Parameter<float> boreal_parameter = Boreal::PARAMETERS<float>[parameter_num];

        setRange (boreal_parameter.min_value, boreal_parameter.max_value, interval);
        if (! juce::approximatelyEqual (skewFactor, 1.0))
            setSkewFactor (skewFactor);
        setScrollWheelEnabled (true);
        setLookAndFeel (&faderLnf);
        setDoubleClickReturnValue (true, (double) boreal_parameter.default_value);

        mAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>
                      (state_to_control, boreal_parameter.ID, *this);
    }

    ~SquareFader() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        const float zs = BorealZoom::uiScale;

        auto track = getLookAndFeel().getSliderLayout (*this).sliderBounds.toFloat();
        auto capArea = getLocalBounds().toFloat();
        if (faderLnf.captionBelow)
            capArea = capArea.removeFromBottom (17.0f * zs);
        else
            capArea = capArea.removeFromLeft (capWidthU * zs);
        if (track.getWidth() <= 0.0f || track.getHeight() <= 0.0f)
            return;
        const float corner = 3.0f * zs;

        g.setColour (GUI::Color::Background);
        g.fillRoundedRectangle (track, corner);

        const double proportion
            = juce::jlimit (0.0, 1.0, valueToProportionOfLength (getValue()));
        if (proportion > 0.0)
        {
            const float fillH = (float) proportion * track.getHeight();
            const auto fill = track.withTrimmedTop (track.getHeight() - fillH);
            g.saveState();
            g.reduceClipRegion (juce::Rectangle<int> (track.toNearestInt()));
            g.setColour (GUI::Color::Logo.withAlpha (
                isMouseOverOrDragging() ? 0.38f : 0.28f));
            g.fillRect (fill);
            g.restoreState();
        }

        g.setColour (GUI::Color::Logo.withAlpha (0.60f));
        g.drawRoundedRectangle (track, corner, 1.0f * zs);

        g.setColour (GUI::Color::KeyDown);
        {
            const juce::String text = valueFormatter
                ? valueFormatter (getValue())
                : (decimals == 0
                    ? juce::String (juce::roundToInt (getValue())) + unit
                    : juce::String (getValue(), decimals) + unit);
            if (verticalValue)
            {
                const float availL = track.getHeight() - 4.0f * zs;
                float valueSize = 12.0f * fontScale;
                juce::Font f = CustomLookAndFeel::makeFont (valueSize);
                while (valueSize > 9.5f
                       && juce::GlyphArrangement::getStringWidthInt (f, text) > juce::roundToInt (availL))
                {
                    valueSize -= 0.5f;
                    f = CustomLookAndFeel::makeFont (valueSize);
                }
                g.saveState();
                g.addTransform (juce::AffineTransform::rotation (
                    -juce::MathConstants<float>::pi * 0.5f,
                    track.getCentreX(), track.getCentreY()));
                g.setFont (f);
                g.drawText (text,
                            track.withSizeKeepingCentre (track.getHeight(), track.getWidth()),
                            juce::Justification::centred, false);
                g.restoreState();
            }
            else
            {
                const float availW = track.getWidth() - 4.0f * zs;
                float valueSize = 12.0f * fontScale;
                juce::Font f = CustomLookAndFeel::makeFont (valueSize);
                while (valueSize > 9.5f
                       && juce::GlyphArrangement::getStringWidthInt (f, text) > juce::roundToInt (availW))
                {
                    valueSize -= 0.5f;
                    f = CustomLookAndFeel::makeFont (valueSize);
                }
                g.setFont (f);
                g.drawText (text, track, juce::Justification::centred, false);
            }
        }

        if (faderLnf.captionBelow)
        {
            g.setColour (GUI::Color::Logo.withAlpha (0.75f));
            g.setFont (CustomLookAndFeel::makeFont (14.0f * fontScale * captionScale));
            g.drawText (faderName,
                        capArea.translated (0.0f, 2.0f * zs),
                        juce::Justification::centred, false);
            return;
        }

        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (
            -juce::MathConstants<float>::pi * 0.5f,
            capArea.getCentreX(), capArea.getCentreY()));
        g.setColour (GUI::Color::Logo.withAlpha (0.75f));
        g.setFont (CustomLookAndFeel::makeFont (14.0f * fontScale * captionScale));
        g.drawText (faderName,
                    capArea.withSizeKeepingCentre (capArea.getHeight(), capArea.getWidth()),
                    juce::Justification::centred, false);
        g.restoreState();
    }

    void refresh()
    {
        setValue ((double) boundValue.getValue(),
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

    void setCaptionScale (float s) noexcept
    {
        if (captionScale != s)
        {
            captionScale = s;
            repaint();
        }
    }

    void setSquareTrack (bool shouldBeSquare) noexcept
    {
        if (faderLnf.squareTrack != shouldBeSquare)
        {
            faderLnf.squareTrack = shouldBeSquare;
            repaint();
        }
    }

    void setVerticalValue (bool shouldBeVertical) noexcept
    {
        if (verticalValue != shouldBeVertical)
        {
            verticalValue = shouldBeVertical;
            repaint();
        }
    }

    void setCaptionBelow (bool shouldBeBelow) noexcept
    {
        if (faderLnf.captionBelow != shouldBeBelow)
        {
            faderLnf.captionBelow = shouldBeBelow;
            repaint();
        }
    }

    void setTrackInsetU (float units) noexcept
    {
        if (faderLnf.trackSideInsetU != units)
        {
            faderLnf.trackSideInsetU = units;
            repaint();
        }
    }

private:
    struct FaderLook : public CustomLookAndFeel
    {
        bool squareTrack = false;
        float trackSideInsetU = 0.0f;
        bool captionBelow = false;

        juce::Slider::SliderLayout getSliderLayout (juce::Slider& s) override
        {
            juce::Slider::SliderLayout layout;
            auto track = s.getLocalBounds().toFloat();
            if (captionBelow)
                track.removeFromBottom (17.0f * BorealZoom::uiScale);
            else
                track.removeFromLeft (SquareFader::capWidthU * BorealZoom::uiScale);
            track = track.reduced (2.0f * BorealZoom::uiScale);
            const float sideInset = trackSideInsetU * BorealZoom::uiScale;
            if (sideInset > 0.0f && track.getWidth() > sideInset * 2.0f)
            {
                track.removeFromLeft (sideInset);
                track.removeFromRight (sideInset);
            }
            if (squareTrack)
            {
                const float side = juce::jmin (track.getWidth(), track.getHeight());
                track = track.withSizeKeepingCentre (side, side);
            }
            layout.sliderBounds = track.toNearestInt();
            layout.textBoxBounds = juce::Rectangle<int>();
            return layout;
        }
    };

    void sliderValueChanged (juce::Slider*) override
    {
        boundValue.setValue (getValue());
    }

    void valueChanged() override { repaint(); }

    juce::String faderName;
    juce::String unit;
    juce::Value boundValue;
    int decimals = 1;
    Formatter valueFormatter;
    bool verticalValue = false;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> mAttachment;
    float fontScale = 1.0f;
    float captionScale = 1.0f;
    FaderLook faderLnf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SquareFader)
};
