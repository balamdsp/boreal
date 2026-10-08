#pragma once

#include <JuceHeader.h>
#include <memory>

#include "../Components/Slider.h"
#include "../Components/SquareFader.h"
#include "../Helpers/CustomLookAndFeel.h"
#include "../Helpers/InterfaceDefines.h"
#include "../Helpers/BorealConstants.h"
#include "../PluginProcessor.h"

class SettingsPanel : public Component,
                      private juce::AudioProcessorValueTreeState::Listener,
                      public BorealAudioProcessor::SlotLoadListener
{
public:
    SettingsPanel (BorealAudioProcessor* inProcessor)
    :   mProcessor (inProcessor)
    {
        crossModeButton.setButtonText ("XROSS");
        crossModeButton.setClickingTogglesState (true);
        crossModeButton.setTooltip ("Cross mode -- pitch follows the FREQ slot, loudness the AMP slot (off = classic pad morph)");
        crossModeButton.setLookAndFeel (&smallToggleLnf);
        crossModeButton.onClick = [this]
        {
            if (auto* p = mProcessor->apvts.getParameter (
                    Boreal::PARAMETERS<float>[Boreal::Parameters::cross_mode].ID))
                p->setValueNotifyingHost (crossModeButton.getToggleState() ? 1.0f : 0.0f);
        };
        addAndMakeVisible (crossModeButton);

        auto wireCornerButton = [this] (juce::ToggleButton& b, Boreal::Parameters param,
                                        const char* tag, const char* tip)
        {
            b.setTooltip (tip);
            b.setLookAndFeel (&smallToggleLnf);
            b.onClick = [this, param]
            {
                if (auto* p = mProcessor->apvts.getParameter (
                        Boreal::PARAMETERS<float>[param].ID))
                {
                    const int cur = juce::jlimit (0, 3, juce::roundToInt (p->getValue() * 3.0f));
                    p->setValueNotifyingHost ((float) ((cur + 1) % 4) / 3.0f);
                }
            };
            addAndMakeVisible (b);
            refreshCornerButton (b, param, tag);
        };

        wireCornerButton (crossFreqButton, Boreal::Parameters::cross_freq_corner, "FREQ",
                          "Cross frequency slot -- pitch comes from this slot in Cross mode (1 top-left, 2 top-right, 3 bottom-left, 4 bottom-right)");
        wireCornerButton (crossAmpButton, Boreal::Parameters::cross_amp_corner, "AMP",
                          "Cross amplitude slot -- loudness comes from this slot in Cross mode (1 top-left, 2 top-right, 3 bottom-left, 4 bottom-right)");
        wireCornerButton (crossFormButton, Boreal::Parameters::cross_form_corner, "FORM",
                          "Cross formant slot -- resonance shift follows this slot in Cross mode (1 top-left, 2 top-right, 3 bottom-left, 4 bottom-right; audible only with per-slot FORMANT set)");
        refreshCrossModeButton();

        mProcessor->apvts.addParameterListener (
            Boreal::PARAMETERS<float>[Boreal::Parameters::cross_mode].ID, this);
        mProcessor->apvts.addParameterListener (
            Boreal::PARAMETERS<float>[Boreal::Parameters::cross_freq_corner].ID, this);
        mProcessor->apvts.addParameterListener (
            Boreal::PARAMETERS<float>[Boreal::Parameters::cross_amp_corner].ID, this);
        mProcessor->apvts.addParameterListener (
            Boreal::PARAMETERS<float>[Boreal::Parameters::cross_form_corner].ID, this);
        mProcessor->addSlotLoadListener (this);

        const auto pctText = [] (double v)
        { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };

        morphFreqFader = std::make_unique<SquareFader> (inProcessor->apvts,
                                                        Boreal::Parameters::morph_freq_x,
                                                        "FREQUENCY", "%", 1.0, 0, 0.001, pctText);
        morphFreqFader->setTooltip ("Frequency morph offset -- shifts only the pitch blend away from the pad, so pitch can sit at a different corner mix than loudness (0% = follows the pad)");
        addAndMakeVisible (morphFreqFader.get());

        morphAmpFader = std::make_unique<SquareFader> (inProcessor->apvts,
                                                       Boreal::Parameters::morph_amp_x,
                                                       "AMPLITUDE", "%", 1.0, 0, 0.001, pctText);
        morphAmpFader->setTooltip ("Amplitude morph offset -- shifts only the loudness blend away from the pad (0% = follows the pad)");
        addAndMakeVisible (morphAmpFader.get());

        morphBwFader = std::make_unique<SquareFader> (inProcessor->apvts,
                                                      Boreal::Parameters::morph_bw_x,
                                                      "BANDWIDTH", "%", 1.0, 0, 0.001, pctText);
        morphBwFader->setTooltip ("Bandwidth morph offset -- shifts only the brightness blend away from the pad (0% = follows the pad)");
        addAndMakeVisible (morphBwFader.get());

        for (auto* f : { morphFreqFader.get(), morphAmpFader.get(), morphBwFader.get() })
        {
            f->setFontScale (2.0f);
            f->setCaptionScale (0.7f);
            f->setSquareTrack (false);
            f->setVerticalValue (true);
            f->setTrackInsetU (4.0f);
        }

        transPresLabel.setText ("TRANS PRESERVE", NotificationType::dontSendNotification);
        transPresLabel.setColour (juce::Label::textColourId, GUI::Color::Logo.withAlpha (0.75f));
        transPresLabel.setFont (CustomLookAndFeel::makeFont (19.0f));
        transPresLabel.setJustificationType (Justification::centredLeft);
        addAndMakeVisible (transPresLabel);

        transPresSlider = std::make_unique<Boreal::Slider> (inProcessor->apvts,
                                                            Boreal::Parameters::transient_preserve,
                                                            juce::Slider::LinearHorizontal);
        transPresSlider->setValueBoxFontSize (20.0f);
        transPresSlider->setTooltip ("Transient preserve -- how much of each sound's analyzed attack punch is layered onto notes (0% = pure smooth morph, 100% = full snap)");
        addAndMakeVisible (transPresSlider.get());

        attackFadeLabel.setText ("ATTACK FADE", NotificationType::dontSendNotification);
        attackFadeLabel.setColour (juce::Label::textColourId, GUI::Color::Logo.withAlpha (0.75f));
        attackFadeLabel.setFont (CustomLookAndFeel::makeFont (19.0f));
        attackFadeLabel.setJustificationType (Justification::centredLeft);
        addAndMakeVisible (attackFadeLabel);

        attackFadeSlider = std::make_unique<Boreal::Slider> (inProcessor->apvts,
                                                             Boreal::Parameters::attack_fade_ms,
                                                             juce::Slider::LinearHorizontal);
        attackFadeSlider->setValueBoxFontSize (20.0f);
        attackFadeSlider->setTooltip ("Attack fade -- crossfade length blending the transient snap into the sustained morph (longer = softer attacks)");
        addAndMakeVisible (attackFadeSlider.get());

        hitWindowLabel.setText ("HIT WINDOW", NotificationType::dontSendNotification);
        hitWindowLabel.setColour (juce::Label::textColourId, GUI::Color::Logo.withAlpha (0.75f));
        hitWindowLabel.setFont (CustomLookAndFeel::makeFont (19.0f));
        hitWindowLabel.setJustificationType (Justification::centredLeft);
        addAndMakeVisible (hitWindowLabel);

        hitWindowSlider = std::make_unique<Boreal::Slider> (inProcessor->apvts,
                                                            Boreal::Parameters::attack_window_ms,
                                                            juce::Slider::LinearHorizontal);
        hitWindowSlider->setValueBoxFontSize (20.0f);
        hitWindowSlider->setTooltip ("Hit window -- time around each detected onset that counts as attack material");
        addAndMakeVisible (hitWindowSlider.get());

        preRollLabel.setText ("PRE-ROLL", NotificationType::dontSendNotification);
        preRollLabel.setColour (juce::Label::textColourId, GUI::Color::Logo.withAlpha (0.75f));
        preRollLabel.setFont (CustomLookAndFeel::makeFont (19.0f));
        preRollLabel.setJustificationType (Justification::centredLeft);
        addAndMakeVisible (preRollLabel);

        preRollSlider = std::make_unique<ValueSlider> (inProcessor->analyzerPreRollMsValue, 0.0, 10.0, 0.5);
        preRollSlider->setTooltip ("Pre-roll -- lead time captured before each onset so fast attacks are never clipped");
        addAndMakeVisible (preRollSlider.get());

        tipDelayFader = std::make_unique<SquareFader> (inProcessor->tooltipDelayMsValue,
                                                        "DELAY", 0.0, 2000.0, 50.0, " ms", 1.0, 0);
        tipDelayFader->setTooltip ("Tooltip delay in milliseconds -- 0 shows tips instantly");
        tipDelayFader->setFontScale (1.8f);
        tipDelayFader->setCaptionScale (0.78f);
        tipDelayFader->setSquareTrack (true);
        tipDelayFader->setTrackInsetU (4.0f);
        addAndMakeVisible (tipDelayFader.get());

        tooltipsToggle.setButtonText ("TOOLTIPS");
        tooltipsToggle.setClickingTogglesState (true);
        tooltipsToggle.setTooltip ("Show helper tips when hovering any control");
        tooltipsToggle.getToggleStateValue().referTo (inProcessor->tooltipsEnabledValue);
        tooltipsToggle.setLookAndFeel (&smallToggleLnf);
        addAndMakeVisible (tooltipsToggle);

        confirmToggle.setButtonText ("CONFIRM ON REMOVE");
        confirmToggle.setClickingTogglesState (true);
        confirmToggle.setTooltip ("Confirm before removing slot sounds or clearing analyzer sessions");
        confirmToggle.getToggleStateValue().referTo (inProcessor->confirmDestructiveValue);
        confirmToggle.setLookAndFeel (&smallToggleLnf);
        addAndMakeVisible (confirmToggle);
    }

    ~SettingsPanel()
    {
        mProcessor->removeSlotLoadListener (this);
        mProcessor->apvts.removeParameterListener (
            Boreal::PARAMETERS<float>[Boreal::Parameters::cross_mode].ID, this);
        mProcessor->apvts.removeParameterListener (
            Boreal::PARAMETERS<float>[Boreal::Parameters::cross_freq_corner].ID, this);
        mProcessor->apvts.removeParameterListener (
            Boreal::PARAMETERS<float>[Boreal::Parameters::cross_amp_corner].ID, this);
        mProcessor->apvts.removeParameterListener (
            Boreal::PARAMETERS<float>[Boreal::Parameters::cross_form_corner].ID, this);
        tooltipsToggle.setLookAndFeel (nullptr);
        confirmToggle.setLookAndFeel (nullptr);
        crossModeButton.setLookAndFeel (nullptr);
        crossFreqButton.setLookAndFeel (nullptr);
        crossAmpButton.setLookAndFeel (nullptr);
        crossFormButton.setLookAndFeel (nullptr);
    }

    void paint (Graphics& g) override
    {
        (void) g;
        drawCard (g, morphArea, "MORPH");
        drawCard (g, transArea, "TRANSIENTS & ATTACKS");
        drawCard (g, uiArea, "INTERFACE & BEHAVIOR");
    }

    void resized() override
    {
        const int inset = (int) (GUI::Layout::CardInset * BorealZoom::uiScale);
        const int card_gap = juce::roundToInt (10.0f * BorealZoom::uiScale);

        auto content = getLocalBounds().reduced (inset);

        const int avail = content.getHeight() - card_gap * 2;
        const int ui_height = juce::roundToInt ((float) avail * 0.26f);
        const int morph_height = juce::roundToInt ((float) avail * 0.41f);
        const int trans_height = avail - ui_height - morph_height;

        morphArea = content.removeFromTop (morph_height);
        content.removeFromTop (card_gap);
        transArea = content.removeFromTop (trans_height);
        content.removeFromTop (card_gap);
        uiArea = content;

        {
            const float zs = BorealZoom::uiScale;
            const int side = juce::roundToInt (12.0f * zs);
            const int top = juce::roundToInt (8.0f * zs);
            const int bottom = juce::roundToInt (12.0f * zs);
            const int title_h = juce::roundToInt (20.0f * zs);
            auto morph = morphArea;
            morph.removeFromLeft (side);
            morph.removeFromRight (side);
            morph.removeFromTop (top);
            morph.removeFromBottom (bottom);
            morph.removeFromTop (title_h);

            const int side_w = juce::jmin (juce::roundToInt (120.0f * zs),
                                             morph.getWidth() / 3);
            const int btn_gap = juce::roundToInt (8.0f * zs);
            auto sideCol = morph.removeFromRight (side_w);
            morph.removeFromRight (juce::roundToInt (10.0f * zs));

            const int btn_h = (sideCol.getHeight() - btn_gap * 3) / 4;
            crossModeButton.setBounds (sideCol.removeFromTop (btn_h));
            sideCol.removeFromTop (btn_gap);
            crossFreqButton.setBounds (sideCol.removeFromTop (btn_h));
            sideCol.removeFromTop (btn_gap);
            crossAmpButton.setBounds (sideCol.removeFromTop (btn_h));
            sideCol.removeFromTop (btn_gap);
            crossFormButton.setBounds (sideCol);

            auto faders = morph;
            const int morph_gap = juce::roundToInt (12.0f * zs);
            const int fw = (faders.getWidth() - morph_gap * 2) / 3;
            morphFreqFader->setBounds (faders.removeFromLeft (fw));
            faders.removeFromLeft (morph_gap);
            morphAmpFader->setBounds (faders.removeFromLeft (fw));
            faders.removeFromLeft (morph_gap);
            morphBwFader->setBounds (faders);
        }

        {
            const float zs = BorealZoom::uiScale;
            const int side = juce::roundToInt (12.0f * zs);
            const int top = juce::roundToInt (8.0f * zs);
            const int bottom = juce::roundToInt (12.0f * zs);
            const int title_h = juce::roundToInt (20.0f * zs);
            auto ui = uiArea;
            ui.removeFromLeft (side);
            ui.removeFromRight (side);
            ui.removeFromTop (top);
            ui.removeFromBottom (bottom);
            ui.removeFromTop (title_h);

            const int col_gap = juce::roundToInt (10.0f * zs);
            const int fader_w = juce::jmin (juce::roundToInt (110.0f * zs), ui.getWidth() / 2);
            tipDelayFader->setBounds (ui.removeFromLeft (fader_w));
            ui.removeFromLeft (col_gap);

            const int toggle_gap = juce::roundToInt (8.0f * zs);
            const int toggle_h = (ui.getHeight() - toggle_gap) / 2;
            tooltipsToggle.setBounds (ui.removeFromTop (toggle_h));
            ui.removeFromTop (toggle_gap);
            confirmToggle.setBounds (ui);
        }

        juce::Label* transLabels[] = { &transPresLabel, &attackFadeLabel, &hitWindowLabel, &preRollLabel };
        juce::Component* transControls[] = { transPresSlider.get(), attackFadeSlider.get(), hitWindowSlider.get(),
                                             preRollSlider.get() };

        layoutRows (transArea, transLabels, transControls, 4);
    }

private:

    void parameterChanged (const juce::String& parameterID, float) override
    {
        if (parameterID == Boreal::PARAMETERS<float>[Boreal::Parameters::cross_mode].ID)
            refreshCrossModeButton();
        else if (parameterID == Boreal::PARAMETERS<float>[Boreal::Parameters::cross_freq_corner].ID)
            refreshCornerButton (crossFreqButton, Boreal::Parameters::cross_freq_corner, "FREQ");
        else if (parameterID == Boreal::PARAMETERS<float>[Boreal::Parameters::cross_amp_corner].ID)
            refreshCornerButton (crossAmpButton, Boreal::Parameters::cross_amp_corner, "AMP");
        else if (parameterID == Boreal::PARAMETERS<float>[Boreal::Parameters::cross_form_corner].ID)
            refreshCornerButton (crossFormButton, Boreal::Parameters::cross_form_corner, "FORM");
    }

    void refreshCrossModeButton()
    {
        const float mode = Boreal::getParameterValueSafe<float> (
            &mProcessor->apvts,
            Boreal::PARAMETERS<float>[Boreal::Parameters::cross_mode].ID, 0.0f);
        const bool xOn = mode >= 0.5f;
        crossModeButton.setToggleState (xOn, NotificationType::dontSendNotification);
        for (auto* b : { &crossFreqButton, &crossAmpButton, &crossFormButton })
        {
            b->setEnabled (xOn);
            b->setAlpha (xOn ? 1.0f : 0.6f);
        }
    }

    void refreshCornerButton (juce::ToggleButton& b, Boreal::Parameters param, const char* tag)
    {
        const int idx = juce::jlimit (0, 3, juce::roundToInt (
            Boreal::getParameterValueSafe<float> (
                &mProcessor->apvts, Boreal::PARAMETERS<float>[param].ID, 0.0f)));
        b.setButtonText (juce::String (tag) + " " + juce::String (idx + 1));
        b.setToggleState ((mProcessor->getSlotLoadMask() & (1 << idx)) != 0,
                          juce::NotificationType::dontSendNotification);
    }

    void slotLoadFinished (int, bool, const juce::String&) override
    {
        refreshCornerButton (crossFreqButton, Boreal::Parameters::cross_freq_corner, "FREQ");
        refreshCornerButton (crossAmpButton, Boreal::Parameters::cross_amp_corner, "AMP");
        refreshCornerButton (crossFormButton, Boreal::Parameters::cross_form_corner, "FORM");
    }

    struct SmallToggleLook : public CustomLookAndFeel
    {
        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                               bool shouldDrawButtonAsHighlighted, bool isButtonDown) override
        {
            drawButtonBackground (g, button, findColour (juce::TextButton::buttonColourId),
                                  shouldDrawButtonAsHighlighted, isButtonDown);

            using namespace BorealColors;
            const bool isOn = button.getToggleState();
            const auto area = button.getLocalBounds();
            g.setFont (CustomLookAndFeel::makeFont (20.0f));
            g.setColour (isOn ? textPrimary : (shouldDrawButtonAsHighlighted ? textPrimary : textMid));

            const juce::String text = isOn ? ("> " + button.getButtonText() + " <")
                                           : ("[ " + button.getButtonText() + " ]");

            g.drawText (text, area.translated (0, juce::roundToInt (-2.0f * BorealZoom::uiScale)),
                        juce::Justification::centred, true);
        }
    };

    struct ValueSlider : public juce::Slider, private juce::Slider::Listener
    {
        ValueSlider (juce::Value source, double lo, double hi, double step)
        :   juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight),
            boundValue (source)
        {
            setRange (lo, hi, step);
            setColour (textBoxOutlineColourId, Colours::transparentBlack);
            setColour (textBoxHighlightColourId, GUI::Color::AccentDim);
            setValue ((double) boundValue.getValue(), NotificationType::dontSendNotification);
            addListener (this);
        }

        void lookAndFeelChanged() override
        {
            juce::Slider::lookAndFeelChanged();
            for (auto* child : getChildren())
                if (auto* box = dynamic_cast<juce::Label*> (child))
                    box->setFont (CustomLookAndFeel::makeFont (20.0f));
        }

        void sliderValueChanged (juce::Slider*) override { boundValue.setValue (getValue()); }

        juce::Value boundValue;
    };

    void drawCard (Graphics& g, const Rectangle<int>& area, const String& title)
    {
        if (area.getWidth() <= 0 || area.getHeight() <= 0)
            return;

        const float innerCornerSize = GUI::Layout::InnerCardCorner * BorealZoom::uiScale;

        const auto card = GUI::Paint::insetCardBounds (area.toFloat());
        g.setColour (GUI::Color::Background);
        g.fillRoundedRectangle (card, innerCornerSize);
        GUI::Paint::drawCardOutline (g, area.toFloat(), innerCornerSize);

        g.setColour (GUI::Color::KeyDown);
        g.setFont (CustomLookAndFeel::makeFont (18.0f));
        g.drawText (">> " + title, area.reduced (juce::roundToInt (6.0f * BorealZoom::uiScale), 0)
                        .withTrimmedTop (juce::roundToInt (2.0f * BorealZoom::uiScale))
                        .withHeight (juce::roundToInt (20.0f * BorealZoom::uiScale)),
                    Justification::topLeft, false);
    }

    void layoutRows (Rectangle<int> card,
                     juce::Label** labels,
                     juce::Component** controls,
                     int numRows)
    {
        const int label_width = juce::roundToInt (140.0f * BorealZoom::uiScale);
        const int row_height = juce::roundToInt (30.0f * BorealZoom::uiScale);
        const int gap = juce::roundToInt (8.0f * BorealZoom::uiScale);

        const int side_pad = juce::roundToInt (12.0f * BorealZoom::uiScale);
        const int top_pad = juce::roundToInt (20.0f * BorealZoom::uiScale)
                          + juce::roundToInt (2.0f * BorealZoom::uiScale)
                          + juce::roundToInt (8.0f * BorealZoom::uiScale);
        const int bottom_pad = juce::roundToInt (12.0f * BorealZoom::uiScale);

        const int rows_y = card.getY() + top_pad;
        const int rows_height = card.getHeight() - top_pad - bottom_pad;

        const int totalGaps = (numRows - 1) * gap;
        const int rowSpan = jmax (juce::roundToInt (26.0f * BorealZoom::uiScale),
                                  (rows_height - totalGaps) / jmax (1, numRows));
        const int rowStep = rowSpan + gap;

        const int rows_x = card.getX() + side_pad;
        const int rows_width = card.getWidth() - side_pad * 2;

        for (int i = 0; i < numRows; ++i)
        {
            const int row_y = rows_y + i * rowStep;

            FlexBox row;
            row.flexDirection = FlexBox::Direction::row;
            row.justifyContent = FlexBox::JustifyContent::flexStart;

            if (labels[i] == nullptr)
            {
                row.items.add (FlexItem (*controls[i]).withFlex (1.0f));
            }
            else
            {
                row.items.add (FlexItem (static_cast<float> (label_width), static_cast<float> (row_height), *labels[i]).withMargin ({ -3.0f, 8.0f, 0.0f, 0.0f }));
                row.items.add (FlexItem (*controls[i]).withFlex (1.0f));
            }

            row.performLayout (Rectangle<int> (rows_x, row_y, rows_width, rowSpan));
        }
    }

    BorealAudioProcessor* mProcessor;

    Rectangle<int> morphArea;
    Rectangle<int> transArea;
    Rectangle<int> uiArea;

    std::unique_ptr<SquareFader> morphFreqFader;
    std::unique_ptr<SquareFader> morphAmpFader;
    std::unique_ptr<SquareFader> morphBwFader;

    juce::ToggleButton crossModeButton;
    juce::ToggleButton crossFreqButton;
    juce::ToggleButton crossAmpButton;
    juce::ToggleButton crossFormButton;

    juce::Label transPresLabel;
    std::unique_ptr<Boreal::Slider> transPresSlider;
    juce::Label attackFadeLabel;
    std::unique_ptr<Boreal::Slider> attackFadeSlider;
    juce::Label hitWindowLabel;
    std::unique_ptr<Boreal::Slider> hitWindowSlider;

    juce::Label preRollLabel;
    std::unique_ptr<ValueSlider> preRollSlider;

    std::unique_ptr<SquareFader> tipDelayFader;
    juce::ToggleButton tooltipsToggle;
    juce::ToggleButton confirmToggle;

    SmallToggleLook smallToggleLnf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsPanel)
};
