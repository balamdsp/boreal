#pragma once

#include <JuceHeader.h>
#include <memory>

#include "../Helpers/InterfaceDefines.h"
#include "../Helpers/BorealParameters.h"
#include "../Helpers/BorealConstants.h"

namespace Boreal { class Slider; }

class Boreal::Slider : public juce::Slider
{
public:

    Slider (AudioProcessorValueTreeState& state_to_control,
            const Boreal::Parameters parameter_num,
            const Slider::SliderStyle slider_style = Slider::SliderStyle::RotaryHorizontalVerticalDrag)
    :   juce::Slider (),
        mParameterNum (parameter_num)
    {
        Boreal::Parameter<float> boreal_parameter = Boreal::PARAMETERS<float>[parameter_num];

        setName (boreal_parameter.label);

        setSliderStyle (slider_style);
        applyTextBoxStyle();

        setRange (boreal_parameter.min_value, boreal_parameter.max_value, 0.001f);
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour (Slider::textBoxHighlightColourId, GUI::Color::AccentDim);

        mAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>
                      (state_to_control, boreal_parameter.ID, *this);

        updateText();
    }

    ~Slider() override {}

    void resized() override
    {
        juce::Slider::resized();
        if (! juce::approximatelyEqual (BorealZoom::uiScale, lastTextBoxScale))
            applyTextBoxStyle();
    }

    void lookAndFeelChanged() override
    {
        juce::Slider::lookAndFeelChanged();
        for (auto* child : getChildren())
            if (auto* box = dynamic_cast<juce::Label*> (child))
                box->setFont (CustomLookAndFeel::makeFont (valueBoxFontSize));
    }

    void setValueBoxFontSize (float s) noexcept
    {
        valueBoxFontSize = s;
        lookAndFeelChanged();
    }

    String getTextFromValue (double value) override
    {
        if (mParameterNum >= Boreal::Parameters::slot1_formant
            && mParameterNum <= Boreal::Parameters::slot4_formant)
            return String (value, 1) + " st";

        if (mParameterNum >= Boreal::Parameters::slot1_rate
            && mParameterNum <= Boreal::Parameters::slot4_reverse)
            return String (roundToInt (value)) + "%";

        switch (mParameterNum)
        {
            case Boreal::Parameters::OutputGain:
                if (value <= Boreal::PARAMETERS<float>[Boreal::Parameters::OutputGain].min_value)
                    return "-inf";
                if (std::abs (value - std::round (value)) < 0.001)
                    return String (roundToInt (value)) + " dB";

                return String (value, 1) + " dB";

            case Boreal::Parameters::asdr_attack:
            case Boreal::Parameters::asdr_decay:
            case Boreal::Parameters::asdr_release:
                return String (value, 2) + " s";

            case Boreal::Parameters::asdr_sustain:
                return String (value, 2);

            case Boreal::Parameters::time_scrub_rate:
                return String (roundToInt (value)) + "%";

            case Boreal::Parameters::pitch_bend_range:
                return String (roundToInt (value)) + " st";

            case Boreal::Parameters::morph_freq_x:
            case Boreal::Parameters::morph_amp_x:
            case Boreal::Parameters::morph_bw_x:
                return String (roundToInt (value * 100.0f)) + "%";

            case Boreal::Parameters::pad_smoothing_ms:
            case Boreal::Parameters::glide_time_ms:
                return String (roundToInt (value)) + " ms";

            case Boreal::Parameters::transient_preserve:
                return String (roundToInt (value)) + "%";

            case Boreal::Parameters::attack_fade_ms:
            case Boreal::Parameters::attack_window_ms:
                return String (roundToInt (value)) + " ms";

            case Boreal::Parameters::freqs_interp_factor:
            case Boreal::Parameters::mags_interp_factor:
            case Boreal::Parameters::TotalNumParameters:
            default:
                return juce::Slider::getTextFromValue (value);
        }
    }

    double getValueFromText (const String& text) override
    {
        if (mParameterNum == Boreal::Parameters::OutputGain)
            return text.retainCharacters ("-+.0123456789").getDoubleValue();

        return juce::Slider::getValueFromText (text);
    }

private:

    void applyTextBoxStyle()
    {
        lastTextBoxScale = BorealZoom::uiScale;
        if (getSliderStyle() == Slider::SliderStyle::LinearHorizontal)
            setTextBoxStyle (Slider::TextEntryBoxPosition::TextBoxRight, false,
                             juce::roundToInt (60.0f * lastTextBoxScale),
                             juce::roundToInt (24.0f * lastTextBoxScale));
        else
            setTextBoxStyle (Slider::TextEntryBoxPosition::TextBoxBelow, false, 0, 0);
    }

    float lastTextBoxScale = 0.0f;
    float valueBoxFontSize = 20.0f;

    const Boreal::Parameters mParameterNum;

    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> mAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Slider);
};
