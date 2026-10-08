#pragma once

#include <JuceHeader.h>

namespace Boreal
{
    template<typename T>
    struct Parameter
    {
        String ID;
        String label;
        T min_value;
        T max_value;
        T default_value;
    };

    enum Parameters
    {
        OutputGain = 0,
        freqs_interp_factor,
        mags_interp_factor,
        asdr_attack,
        asdr_decay,
        asdr_sustain,
        asdr_release,
        time_scrub_rate,
        pitch_bend_range,
        morph_freq_x,
        morph_amp_x,
        morph_bw_x,
        pad_smoothing_ms,
        glide_time_ms,
        transient_preserve,
        attack_fade_ms,
        attack_window_ms,
        slot1_rate,
        slot1_offset,
        slot1_loopstart,
        slot1_loopend,
        slot1_reverse,
        slot2_rate,
        slot2_offset,
        slot2_loopstart,
        slot2_loopend,
        slot2_reverse,
        slot3_rate,
        slot3_offset,
        slot3_loopstart,
        slot3_loopend,
        slot3_reverse,
        slot4_rate,
        slot4_offset,
        slot4_loopstart,
        slot4_loopend,
        slot4_reverse,
        slot1_formant,
        slot2_formant,
        slot3_formant,
        slot4_formant,
        cross_mode,
        cross_freq_corner,
        cross_amp_corner,
        cross_form_corner,
        slot1_loopmode,
        slot2_loopmode,
        slot3_loopmode,
        slot4_loopmode,
        transpose_st,
        fine_tune_cents,
        TotalNumParameters
    };

    template <typename T>
    using IndexedParameters = std::map<int, Parameter<T>>;

    template <typename T>
    static IndexedParameters<T> PARAMETERS =
    {

        {OutputGain,            {"OutputGain",          "Master",                   -60.0f,     12.0f,      0.0f}    },
        {freqs_interp_factor,   {"FreqsInterpFactor",   "Morph X Position",         0.0f,       1.0f,       0.5f}   },
        {mags_interp_factor,    {"MagsInterpFactor",    "Morph Y Position",         0.0f,       1.0f,       0.5f}   },
        {asdr_attack,           {"ADSRAttack",          "Attack",                   0.01f,      5.0f,       0.1f}   },
        {asdr_decay,            {"ADSRDecay",           "Decay",                    0.01f,      2.0f,       0.8f}   },
        {asdr_sustain,          {"ADSRSustain",         "Sustain",                  0.0f,       1.0f,       0.8f}   },
        {asdr_release,          {"ADSRRelease",         "Release",                  0.01f,      5.0f,       0.1f}   },
        {time_scrub_rate,       {"TimeScrubRate",       "Rate",                     -400.0f,    400.0f,     100.0f}  },
        {pitch_bend_range,      {"PitchBendRange",      "Pitch Bend",               1.0f,       24.0f,      2.0f}   },
        {morph_freq_x,          {"MorphFreqWeight",      "Freq Morph Weight",        -0.5f,      0.5f,       0.0f}   },
        {morph_amp_x,           {"MorphAmpWeight",       "Amp Morph Weight",         -0.5f,      0.5f,       0.0f}   },
        {morph_bw_x,            {"MorphBwWeight",        "BW Morph Weight",          -0.5f,      0.5f,       0.0f}   },
        {pad_smoothing_ms,      {"PadSmoothingTime",     "Pad Smoothing",            0.0f,       1000.0f,    0.0f}   },
        {glide_time_ms,         {"GlideTime",            "Glide",                    0.0f,       1000.0f,    0.0f}   },
        {transient_preserve,    {"TransientPreserve",    "Trans Preserve",           0.0f,       100.0f,     0.0f} },
        {attack_fade_ms,      {"AttackFadeMs",         "Attack Fade",              1.0f,       200.0f,     30.0f}  },
        {attack_window_ms,    {"AttackWindowMs",       "Hit Window",               10.0f,      1000.0f,    300.0f} },
        {slot1_rate,          {"Slot1Rate",            "Slot 1 Rate",              0.0f,       200.0f,     100.0f} },
        {slot1_offset,        {"Slot1Offset",          "Slot 1 Offset",            0.0f,       100.0f,     0.0f}   },
        {slot1_loopstart,     {"Slot1LoopStart",       "Slot 1 Loop Start",        0.0f,       100.0f,     0.0f}   },
        {slot1_loopend,       {"Slot1LoopEnd",         "Slot 1 Loop End",          0.0f,       100.0f,     100.0f} },
        {slot1_reverse,       {"Slot1Reverse",         "Slot 1 Reverse",           0.0f,       1.0f,       0.0f}   },
        {slot2_rate,          {"Slot2Rate",            "Slot 2 Rate",              0.0f,       200.0f,     100.0f} },
        {slot2_offset,        {"Slot2Offset",          "Slot 2 Offset",            0.0f,       100.0f,     0.0f}   },
        {slot2_loopstart,     {"Slot2LoopStart",       "Slot 2 Loop Start",        0.0f,       100.0f,     0.0f}   },
        {slot2_loopend,       {"Slot2LoopEnd",         "Slot 2 Loop End",          0.0f,       100.0f,     100.0f} },
        {slot2_reverse,       {"Slot2Reverse",         "Slot 2 Reverse",           0.0f,       1.0f,       0.0f}   },
        {slot3_rate,          {"Slot3Rate",            "Slot 3 Rate",              0.0f,       200.0f,     100.0f} },
        {slot3_offset,        {"Slot3Offset",          "Slot 3 Offset",            0.0f,       100.0f,     0.0f}   },
        {slot3_loopstart,     {"Slot3LoopStart",       "Slot 3 Loop Start",        0.0f,       100.0f,     0.0f}   },
        {slot3_loopend,       {"Slot3LoopEnd",         "Slot 3 Loop End",          0.0f,       100.0f,     100.0f} },
        {slot3_reverse,       {"Slot3Reverse",         "Slot 3 Reverse",           0.0f,       1.0f,       0.0f}   },
        {slot4_rate,          {"Slot4Rate",            "Slot 4 Rate",              0.0f,       200.0f,     100.0f} },
        {slot4_offset,        {"Slot4Offset",          "Slot 4 Offset",            0.0f,       100.0f,     0.0f}   },
        {slot4_loopstart,     {"Slot4LoopStart",       "Slot 4 Loop Start",        0.0f,       100.0f,     0.0f}   },
        {slot4_loopend,       {"Slot4LoopEnd",         "Slot 4 Loop End",          0.0f,       100.0f,     100.0f} },
        {slot4_reverse,       {"Slot4Reverse",         "Slot 4 Reverse",           0.0f,       1.0f,       0.0f}   },
        {slot1_formant,       {"Slot1Formant",         "Slot 1 Formant",           -12.0f,    12.0f,      0.0f}   },
        {slot2_formant,       {"Slot2Formant",         "Slot 2 Formant",           -12.0f,    12.0f,      0.0f}   },
        {slot3_formant,       {"Slot3Formant",         "Slot 3 Formant",           -12.0f,    12.0f,      0.0f}   },
        {slot4_formant,       {"Slot4Formant",         "Slot 4 Formant",           -12.0f,    12.0f,      0.0f}   },
        {cross_mode,          {"CrossMode",            "Cross Mode",               0.0f,       1.0f,       0.0f}   },
        {cross_freq_corner,   {"CrossFreqCorner",      "Cross Freq Corner",        0.0f,       3.0f,       0.0f}   },
        {cross_amp_corner,    {"CrossAmpCorner",       "Cross Amp Corner",         0.0f,       3.0f,       3.0f}   },
        {cross_form_corner,   {"CrossFormCorner",      "Cross Form Corner",        0.0f,       3.0f,       3.0f}   },
        {slot1_loopmode,      {"Slot1LoopMode",        "Slot 1 Loop",              0.0f,       1.0f,       1.0f}   },
        {slot2_loopmode,      {"Slot2LoopMode",        "Slot 2 Loop",              0.0f,       1.0f,       1.0f}   },
        {slot3_loopmode,      {"Slot3LoopMode",        "Slot 3 Loop",              0.0f,       1.0f,       1.0f}   },
        {slot4_loopmode,      {"Slot4LoopMode",        "Slot 4 Loop",              0.0f,       1.0f,       1.0f}   },
        {transpose_st,        {"TransposeSt",          "Transpose",                -12.0f,    12.0f,      0.0f}   },
        {fine_tune_cents,     {"FineTuneCents",        "Fine Tune",                -100.0f,   100.0f,     0.0f}   },
    };

    template<typename T>
    inline Parameter<T> getParameterByID(String parameter_ID)
    {
        Parameter<T> found_parameter;

        auto result = std::find_if( PARAMETERS<T>.begin(), PARAMETERS<T>.end(), [parameter_ID](const auto& mo)
        {
            return mo.second.ID == parameter_ID;
        });

        if(result != PARAMETERS<T>.end())
            found_parameter = result->second;

        return found_parameter;
    }

    namespace Zoom
    {
        inline constexpr const char* UI_SCALE_ID = "ui_scale";
        inline constexpr std::array<float, 5> ZOOM_PERCENTS { 100.0f, 125.0f, 150.0f, 200.0f, 300.0f };
        inline constexpr int UI_SCALE_DEFAULT = 0;
        inline constexpr float ZOOM_MIN = 1.0f;
        inline constexpr float ZOOM_MAX = 3.0f;
    }

    template<typename T>
    inline T getParameterValueSafe(AudioProcessorValueTreeState* params, const String& paramID, T defaultValue)
    {
        auto param = params->getParameter(paramID);
        if (param != nullptr)
            return (T)param->convertFrom0to1(param->getValue());
        return defaultValue;
    }
}
