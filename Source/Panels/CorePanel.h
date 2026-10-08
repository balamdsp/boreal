#pragma once

#include <JuceHeader.h>
#include <memory>

#include "../Components/PadXY.h"
#include "../Components/SquareFader.h"
#include "../Components/SquareKnob.h"
#include "../Components/BipolarFader.h"
#include "../Components/TransportIconButton.h"
#include "../Helpers/CustomLookAndFeel.h"
#include "../Helpers/InterfaceDefines.h"
#include "../Helpers/BorealConstants.h"
#include "../PluginProcessor.h"

class CorePanel    : public Component,
                     public juce::Value::Listener,
                     public juce::Button::Listener
{
public:
    CorePanel (BorealAudioProcessor* inProcessor)
    :   mProcessor (inProcessor)
    {
        voicesFader = std::make_unique<SquareFader> (mProcessor->voicesValue,
                                                       "VOICES", 1.0, 8.0, 1.0, juce::String(), 1.0, 0);
        voicesFader->setTooltip ("Polyphony -- how many notes sound at once (1 = mono with legato glide, up to 8)");
        voicesFader->setFontScale (2.0f);
        voicesFader->setCaptionScale (0.7f);
        voicesFader->setCaptionBelow (true);
        voicesFader->setSquareTrack (true);
        addAndMakeVisible (voicesFader.get());
        mProcessor->voicesValue.addListener (this);

        bendFader = std::make_unique<SquareFader> (inProcessor->apvts,
                                                   Boreal::Parameters::pitch_bend_range,
                                                   "BEND", " st", 1.0, 0, 1.0);
        bendFader->setTooltip ("Pitch bend range -- how far the pitch wheel bends notes, in semitones (wider = deeper dives)");
        bendFader->setFontScale (2.0f);
        bendFader->setCaptionScale (0.7f);
        bendFader->setCaptionBelow (true);
        bendFader->setSquareTrack (true);
        addAndMakeVisible (bendFader.get());

        glideFader = std::make_unique<SquareFader> (inProcessor->apvts,
                                                    Boreal::Parameters::glide_time_ms,
                                                    "GLIDE", " ms", 1.0, 0);
        glideFader->setTooltip ("Glide -- time the pitch slides between notes (mono legato; longer = lazier portamento)");
        glideFader->setFontScale (2.0f);
        glideFader->setCaptionScale (0.7f);
        glideFader->setCaptionBelow (true);
        glideFader->setSquareTrack (true);
        addAndMakeVisible (glideFader.get());

        transposeFader = std::make_unique<SquareFader> (inProcessor->apvts,
                                                        Boreal::Parameters::transpose_st,
                                                        "TRANSPOSE", " st", 1.0, 0, 1.0);
        transposeFader->setTooltip ("Transpose -- shifts every note by semitones (12 = one octave up)");
        transposeFader->setFontScale (2.0f);
        transposeFader->setCaptionScale (0.7f);
        transposeFader->setCaptionBelow (true);
        transposeFader->setSquareTrack (true);
        addAndMakeVisible (transposeFader.get());

        fineFader = std::make_unique<SquareFader> (inProcessor->apvts,
                                                   Boreal::Parameters::fine_tune_cents,
                                                   "FINE", " ct", 1.0, 0, 1.0);
        fineFader->setTooltip ("Fine tune -- nudges every note by cents (100 = one semitone, for matching other instruments)");
        fineFader->setFontScale (2.0f);
        fineFader->setCaptionScale (0.7f);
        fineFader->setCaptionBelow (true);
        fineFader->setSquareTrack (true);
        addAndMakeVisible (fineFader.get());

        legatoButton.setButtonText ("LEGATO");
        legatoButton.setClickingTogglesState (true);
        legatoButton.setTooltip ("Legato -- in mono, new notes glide from the current pitch instead of retriggering the envelope");
        legatoButton.setToggleState (mProcessor->getLegatoEnabled(), NotificationType::dontSendNotification);
        legatoButton.onClick = [this] { mProcessor->setLegatoEnabled (legatoButton.getToggleState()); };
        legatoButton.setLookAndFeel (&voiceToggleLnf);
        mProcessor->legatoValue.addListener (this);
        addAndMakeVisible (legatoButton);
        applyLegatoAvailability();

        pitchLockButton.setButtonText ("PITCH LOCK");
        pitchLockButton.setClickingTogglesState (true);
        pitchLockButton.setTooltip ("Pitch lock -- pins tuning to C4 concert pitch instead of following the morphed reference (on = in tune with other instruments)");
        pitchLockButton.setToggleState (mProcessor->getPitchLockEnabled(), NotificationType::dontSendNotification);
        pitchLockButton.onClick = [this] { mProcessor->setPitchLockEnabled (pitchLockButton.getToggleState()); };
        pitchLockButton.setLookAndFeel (&voiceToggleLnf);
        mProcessor->pitchLockValue.addListener (this);

        addAndMakeVisible (pitchLockButton);

        rateCaption.setText ("RATE", NotificationType::dontSendNotification);
        rateCaption.setColour (juce::Label::textColourId, GUI::Color::Logo.withAlpha (0.75f));
        rateCaption.setFont (CustomLookAndFeel::makeFont (18.0f));
        rateCaption.setJustificationType (Justification::centred);
        addAndMakeVisible (rateCaption);

        rateFader = std::make_unique<BipolarFader> (inProcessor->apvts,
                                                    Boreal::Parameters::time_scrub_rate, 1.0);
        rateFader->setMouseDragSensitivity (400);
        rateFader->setFontScale (1.4f);
        rateFader->setTooltip ("Playback rate as % of the recorded speed -- 100% sounds natural, 0% freezes time, negative values run backwards");
        addAndMakeVisible (rateFader.get());

        scrubButton.setTooltip ("Rate switch -- off forces 100% natural speed, ignoring the Rate fader");
        scrubButton.setToggleState (mProcessor->getTimeScrubEnabled(), NotificationType::dontSendNotification);
        scrubButton.onClick = [this] { mProcessor->setTimeScrubEnabled (scrubButton.getToggleState()); };
        mProcessor->timeScrubValue.addListener (this);
        addAndMakeVisible (scrubButton);

        loopButton.setTooltip ("Loop switch -- off plays each note once through the sound, then releases");
        loopButton.setToggleState (mProcessor->getLoopEnabled(), NotificationType::dontSendNotification);
        loopButton.onClick = [this]
        {
            mProcessor->setLoopEnabled (loopButton.getToggleState());

            forwardButton.setEnabled (loopButton.getToggleState());
        };
        mProcessor->loopValue.addListener (this);
        addAndMakeVisible (loopButton);

        forwardButton.setTooltip ("Forward-only -- looping always runs forward instead of ping-pong (needs Loop on)");
        forwardButton.setToggleState (mProcessor->getForwardOnlyEnabled(), NotificationType::dontSendNotification);
        forwardButton.onClick = [this] { mProcessor->setForwardOnlyEnabled (forwardButton.getToggleState()); };
        mProcessor->forwardOnlyValue.addListener (this);
        addAndMakeVisible (forwardButton);
        forwardButton.setEnabled (mProcessor->getLoopEnabled());

        const auto padMsText = [] (double v)
        { return juce::String (juce::roundToInt (v)) + " ms"; };
        padKnob = std::make_unique<SquareKnob> (inProcessor->apvts,
                                                Boreal::Parameters::pad_smoothing_ms,
                                                "SMOOTHING", padMsText);
        padKnob->setTooltip ("Pad smoothing -- the XY position glides toward your mouse cursor over this time, so quick pad moves morph gradually instead of jumping");
        padKnob->setFontScale (1.2f);
        addAndMakeVisible (padKnob.get());

        Boreal::Parameter freqs_interp_factor_parameter = Boreal::PARAMETERS<float>[Boreal::Parameters::freqs_interp_factor];
        Boreal::Parameter mags_interp_factor_parameter = Boreal::PARAMETERS<float>[Boreal::Parameters::mags_interp_factor];

        mPadXY = std::make_unique<PadXY> (inProcessor, inProcessor->apvts,
                                          freqs_interp_factor_parameter,
                                          mags_interp_factor_parameter);

        addAndMakeVisible (mPadXY.get());

        const auto secText = [] (double v)
        { return juce::String (v, 2) + " s"; };
        const auto pct01Text = [] (double v)
        { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };

        attackFader  = std::make_unique<SquareFader> (inProcessor->apvts, Boreal::Parameters::asdr_attack,
                                                      "ATTACK", " s", 1.0, 2, 0.001, secText);
        decayFader   = std::make_unique<SquareFader> (inProcessor->apvts, Boreal::Parameters::asdr_decay,
                                                      "DECAY", " s", 1.0, 2, 0.001, secText);
        sustainFader = std::make_unique<SquareFader> (inProcessor->apvts, Boreal::Parameters::asdr_sustain,
                                                      "SUSTAIN", "%", 1.0, 0, 0.001, pct01Text);
        releaseFader = std::make_unique<SquareFader> (inProcessor->apvts, Boreal::Parameters::asdr_release,
                                                      "RELEASE", " s", 1.0, 2, 0.001, secText);

        attackFader->setTooltip ("Amp attack -- fade-in time from note start to full volume");
        decayFader->setTooltip ("Amp decay -- fall time from full volume down to the sustain level");
        sustainFader->setTooltip ("Amp sustain -- held volume while the note is down (0% = silence)");
        releaseFader->setTooltip ("Amp release -- fade-out time after note-off");

        for (auto* f : { attackFader.get(), decayFader.get(),
                         sustainFader.get(), releaseFader.get() })
        {
            f->setFontScale (2.0f);
            f->setCaptionScale (0.7f);
            f->setSquareTrack (false);
            f->setVerticalValue (true);
            f->setTrackInsetU (4.0f);
            addAndMakeVisible (f);
        }
    }

    ~CorePanel()
    {
        legatoButton.setLookAndFeel (nullptr);
        pitchLockButton.setLookAndFeel (nullptr);
        mProcessor->voicesValue.removeListener (this);
        mProcessor->legatoValue.removeListener (this);
        mProcessor->pitchLockValue.removeListener (this);
        mProcessor->timeScrubValue.removeListener (this);
        mProcessor->loopValue.removeListener (this);
        mProcessor->forwardOnlyValue.removeListener (this);
    }

    void valueChanged (juce::Value& value) override
    {
        if (value.refersToSameSourceAs (mProcessor->voicesValue))
        {
            if (voicesFader != nullptr)
                voicesFader->refresh();
            applyLegatoAvailability();
        }
        else if (value.refersToSameSourceAs (mProcessor->legatoValue))
            legatoButton.setToggleState (mProcessor->getLegatoEnabled(), NotificationType::dontSendNotification);
        else if (value.refersToSameSourceAs (mProcessor->pitchLockValue))
            pitchLockButton.setToggleState (mProcessor->getPitchLockEnabled(), NotificationType::dontSendNotification);
        else if (value.refersToSameSourceAs (mProcessor->timeScrubValue))
            scrubButton.setToggleState (mProcessor->getTimeScrubEnabled(), NotificationType::dontSendNotification);
        else if (value.refersToSameSourceAs (mProcessor->loopValue))
        {
            loopButton.setToggleState (mProcessor->getLoopEnabled(), NotificationType::dontSendNotification);
            forwardButton.setEnabled (mProcessor->getLoopEnabled());
        }
        else if (value.refersToSameSourceAs (mProcessor->forwardOnlyValue))
            forwardButton.setToggleState (mProcessor->getForwardOnlyEnabled(), NotificationType::dontSendNotification);
    }

    void buttonClicked (juce::Button*) override {}

    void applyLegatoAvailability()
    {
        const bool mono = mProcessor->getCurrentVoices() == 1;
        legatoButton.setEnabled (mono);
        legatoButton.setAlpha (mono ? 1.0f : 0.35f);
    }

    void paint (Graphics& g) override
    {
        const float innerCornerSize = GUI::Layout::InnerCardCorner * BorealZoom::uiScale;

        auto drawSection = [&] (const Rectangle<int>& area, const String& label)
        {
            if (area.getWidth() <= 0)
                return;

            const float content_pad = 8.0f * BorealZoom::uiScale;

            const auto card = GUI::Paint::insetCardBounds (area.toFloat());
            g.setColour (GUI::Color::Background);
            g.fillRoundedRectangle (card, innerCornerSize);
            GUI::Paint::drawCardOutline (g, area.toFloat(), innerCornerSize);

            if (label.isNotEmpty())
            {
                g.setColour (GUI::Color::Logo.withAlpha (0.70f));
                g.setFont (CustomLookAndFeel::makeFont (18.0f));
                g.drawText (">> " + label, area.reduced (juce::roundToInt (content_pad / 2.0f), juce::roundToInt (2.0f * BorealZoom::uiScale)).withHeight (juce::roundToInt (24.0f * BorealZoom::uiScale)),
                            Justification::topLeft, false);
            }
        };

        drawSection (padArea, {});
        drawSection (controlsArea, {});
        drawSection (adsrArea, {});
        drawSection (voiceArea, {});
        auto compactTitle = [&] (const Rectangle<int>& area, const String& name)
        {
            g.setColour (GUI::Color::KeyDown);
            g.setFont (CustomLookAndFeel::makeFont (18.0f));
            g.drawText (">> " + name, area.reduced (juce::roundToInt (6.0f * BorealZoom::uiScale), 0)
                            .withTrimmedTop (juce::roundToInt (2.0f * BorealZoom::uiScale))
                            .withHeight (juce::roundToInt (20.0f * BorealZoom::uiScale)),
                        Justification::topLeft, false);
        };
        compactTitle (controlsArea, "PLAYBACK");
        compactTitle (adsrArea, "AMP ENVELOPE");
        compactTitle (voiceArea, "VOICING");
    }

    void resized() override
    {
        const float zs = BorealZoom::uiScale;
        const float inset = GUI::Layout::CardInset * zs;
        const float gap = 20.0f * zs;

        const int mid_height = juce::roundToInt (200.0f * zs);
        const int voice_height = juce::roundToInt (120.0f * zs);

        const int section_x = (int) inset;
        const int section_width = getWidth() - (int) inset * 2;

        const int voice_y = getHeight() - (int) inset - voice_height;
        const int mid_y = voice_y - (int) gap - mid_height;
        const int pad_y = (int) inset;
        const int pad_height = jmax (100, mid_y - (int) gap - pad_y);

        voiceArea.setBounds (section_x, voice_y, section_width, voice_height);
        {
            auto mid = Rectangle<int> (section_x, mid_y, section_width, mid_height);
            const int mid_gap = juce::roundToInt (10.0f * zs);
            const int env_w = (section_width - mid_gap) * 3 / 5;
            adsrArea = mid.removeFromLeft (env_w);
            mid.removeFromLeft (mid_gap);
            controlsArea = mid;
        }
        padArea.setBounds (section_x, pad_y, section_width, pad_height);
        mPadXY->setBounds (padArea);

        const int title_h = juce::roundToInt (20.0f * zs);

        {
            const int env_side = juce::roundToInt (12.0f * zs);
            const int env_top = juce::roundToInt (8.0f * zs);
            const int env_bottom = juce::roundToInt (12.0f * zs);
            auto env = adsrArea;
            env.removeFromLeft (env_side);
            env.removeFromRight (env_side);
            env.removeFromTop (env_top);
            env.removeFromBottom (env_bottom);
            env.removeFromTop (title_h);

            auto faders = env;
            const int env_gap = juce::roundToInt (8.0f * zs);
            const int fw = (faders.getWidth() - env_gap * 3) / 4;
            attackFader->setBounds (faders.removeFromLeft (fw));
            faders.removeFromLeft (env_gap);
            decayFader->setBounds (faders.removeFromLeft (fw));
            faders.removeFromLeft (env_gap);
            sustainFader->setBounds (faders.removeFromLeft (fw));
            faders.removeFromLeft (env_gap);
            releaseFader->setBounds (faders);
        }

        {
            const int gen_side = juce::roundToInt (12.0f * zs);
            const int gen_top = juce::roundToInt (8.0f * zs);
            const int gen_bottom = juce::roundToInt (12.0f * zs);
            auto gen = controlsArea;
            gen.removeFromLeft (gen_side);
            gen.removeFromRight (gen_side);
            gen.removeFromTop (gen_top);
            gen.removeFromBottom (gen_bottom);
            gen.removeFromTop (title_h);

            auto rateCap = gen.removeFromTop (juce::roundToInt (16.0f * zs));
            rateCaption.setBounds (rateCap.translated (0, juce::roundToInt (-2.0f * zs)));
            rateFader->setBounds (gen.removeFromTop (juce::roundToInt (32.0f * zs)));
            gen.removeFromTop (juce::roundToInt (8.0f * zs));

            const int gen_gap = juce::roundToInt (10.0f * zs);
            const int knob_w = juce::roundToInt (80.0f * zs);
            padKnob->setBounds (gen.removeFromRight (knob_w));
            gen.removeFromRight (gen_gap);

            const int icon_h = (gen.getHeight() - gen_gap * 2) / 3;
            scrubButton.setBounds (gen.removeFromTop (icon_h));
            gen.removeFromTop (gen_gap);
            loopButton.setBounds (gen.removeFromTop (icon_h));
            gen.removeFromTop (gen_gap);
            forwardButton.setBounds (gen);
        }

        {
            const int strip_pad = juce::roundToInt (12.0f * zs);
            const int strip_gap = juce::roundToInt (6.0f * zs);
            const int strip_top = juce::roundToInt (4.0f * zs);
            const int strip_bottom = juce::roundToInt (12.0f * zs);
            auto strip = voiceArea;
            strip.removeFromLeft (strip_pad);
            strip.removeFromRight (strip_pad);
            strip.removeFromTop (strip_top);
            strip.removeFromBottom (strip_bottom);
            strip.removeFromTop (juce::roundToInt (20.0f * zs));
            strip.removeFromBottom (juce::roundToInt (4.0f * zs));
            const int unit = (strip.getWidth() - strip_gap * 5) / 13;
            const int cell_w = unit * 2;
            const int toggle_w = unit * 3;

            voicesFader->setBounds (strip.removeFromLeft (cell_w));
            strip.removeFromLeft (strip_gap);
            bendFader->setBounds (strip.removeFromLeft (cell_w));
            strip.removeFromLeft (strip_gap);
            glideFader->setBounds (strip.removeFromLeft (cell_w));
            strip.removeFromLeft (strip_gap);
            transposeFader->setBounds (strip.removeFromLeft (cell_w));
            strip.removeFromLeft (strip_gap);
            fineFader->setBounds (strip.removeFromLeft (cell_w));
            strip.removeFromLeft (strip_gap);

            auto toggles = strip.removeFromLeft (toggle_w);
            const int toggle_gap = juce::roundToInt (10.0f * zs);
            const int toggle_h = (toggles.getHeight() - toggle_gap) / 2;
            legatoButton.setBounds (toggles.removeFromTop (toggle_h));
            toggles.removeFromTop (toggle_gap);
            pitchLockButton.setBounds (toggles);
        }
    }

private:

    struct VoiceToggleLook : public CustomLookAndFeel
    {
        juce::Font getTextButtonFont (juce::TextButton&, int) override
        {
            return CustomLookAndFeel::makeFont (20.0f);
        }

        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                               bool shouldDrawButtonAsHighlighted, bool isButtonDown) override
        {
            drawButtonBackground (g, button, findColour (juce::TextButton::buttonColourId),
                                  shouldDrawButtonAsHighlighted, isButtonDown);

            using namespace BorealColors;
            const bool isOn = button.getToggleState();
            const auto area = button.getLocalBounds();
            g.setFont (CustomLookAndFeel::makeFont (20.0f));
            g.setColour (isOn ? GUI::Color::KeyDown : (shouldDrawButtonAsHighlighted ? textPrimary : textMid));

            const juce::String text = isOn ? ("> " + button.getButtonText() + " <")
                                           : ("[ " + button.getButtonText() + " ]");

            g.drawText (text, area.translated (0, juce::roundToInt (-2.0f * BorealZoom::uiScale)),
                        juce::Justification::centred, true);
        }
    };

    BorealAudioProcessor* mProcessor;

    Rectangle<int> controlsArea;
    Rectangle<int> adsrArea;
    Rectangle<int> padArea;
    Rectangle<int> voiceArea;

    std::unique_ptr<SquareFader> voicesFader;
    std::unique_ptr<SquareFader> bendFader;
    std::unique_ptr<SquareFader> glideFader;
    std::unique_ptr<SquareFader> transposeFader;
    std::unique_ptr<SquareFader> fineFader;
    juce::ToggleButton legatoButton;
    juce::ToggleButton pitchLockButton;

    juce::Label rateCaption;
    std::unique_ptr<BipolarFader> rateFader;
    TransportIconButton scrubButton { TransportIconButton::Glyph::Scrub };
    TransportIconButton loopButton { TransportIconButton::Glyph::Loop };
    TransportIconButton forwardButton { TransportIconButton::Glyph::Forward };
    std::unique_ptr<SquareKnob> padKnob;

    std::unique_ptr<PadXY> mPadXY;

    std::unique_ptr<SquareFader> attackFader;
    std::unique_ptr<SquareFader> decayFader;
    std::unique_ptr<SquareFader> sustainFader;
    std::unique_ptr<SquareFader> releaseFader;

    VoiceToggleLook voiceToggleLnf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CorePanel)
};
