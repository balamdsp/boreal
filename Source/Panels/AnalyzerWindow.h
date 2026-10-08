#pragma once

#include <JuceHeader.h>
#include <memory>

#include "../Analysis/BorealAnalyzer.h"

#include "WaveformView.h"
#include "PartialsView.h"
#include "../Components/SquareKnob.h"
#include "../Components/SquareFader.h"

#include "../Helpers/CustomLookAndFeel.h"
#include "../Helpers/InterfaceDefines.h"
#include "../Helpers/BorealConstants.h"
#include "../PluginProcessor.h"

class AnalyzerWindow : public juce::Component,
                       public juce::FileDragAndDropTarget,
                       public juce::DragAndDropTarget,
                       private juce::MultiTimer
{
public:
    explicit AnalyzerWindow (BorealAudioProcessor* inProcessor)
    :   mProcessor (inProcessor),
        sourceView ("SOURCE"),
        resynthView ("RESYNTH")
    {
        addAndMakeVisible (sourceView);
        addAndMakeVisible (partialsView);
        addAndMakeVisible (resynthView);
        addAndMakeVisible (infoLabel);

        sourceView.setPlayheadSupplier ([this] { return mProcessor->previewPlaying() ? mProcessor->previewPlayheadSeconds() : -1.0; });
        resynthView.setPlayheadSupplier ([this] { return mProcessor->previewPlaying() ? mProcessor->previewPlayheadSeconds() : -1.0; });

        sourceView.onIntervalChanged = [this] (double s, double e)
        {

            mProcessor->analyzerSetInterval (s, e);
        };

        sourceView.onEmptyClick = [this] { chooseOpen(); };
        resynthView.setEmptyText ({});
        resynthView.onEmptyClick = [this] { mProcessor->synthesizeScratch(); };

        auto addDial = [&] (std::unique_ptr<SquareKnob>& slot,
                            const char* name, juce::Value bound,
                            double lo, double hi, double step,
                            const char* unit = "", double skew = 1.0,
                            const char* tip = nullptr)
        {
            slot = std::make_unique<SquareKnob> (bound, name, lo, hi, step, unit, skew);
            if (tip != nullptr)
                slot->setTooltip (tip);
            addAndMakeVisible (slot.get());
        };

        addDial (resDial,      "RESOLUTION",   mProcessor->analyzerResolutionValue,     20.0, 1000.0, 1.0, " Hz", 0.35,
                 "Partial tracking resolution -- partials closer than this blend into one (lower hears more detail)");
        addDial (ampFloorDial, "AMP FLOOR",    mProcessor->analyzerAmpFloorValue,      -180.0,  -40.0, 1.0, " dB", 1.0,
                 "Quietest partial kept -- anything below this level is discarded");
        addDial (widthDial,    "WINDOW WIDTH", mProcessor->analyzerWindowWidthHzValue,  50.0, 2000.0, 1.0, " Hz", 0.35,
                 "Analysis window width -- wider hears pitch better, narrower hears timing better");
        addDial (loCutDial,    "LO CUT",       mProcessor->analyzerLoCutHzValue,         0.0, 2000.0, 1.0, " Hz", 0.35,
                 "Ignore everything below this frequency");
        addDial (hiCutDial,    "HI CUT",       mProcessor->analyzerHiCutHzValue,         0.0, 22000.0, 10.0, " Hz", 0.35,
                 "Ignore everything above this frequency");

        addDial (driftDial,    "DRIFT",        mProcessor->analyzerFreqDriftHzValue,     0.0, 2000.0, 1.0, " Hz", 0.35,
                 "How far a partial may wander between frames before it counts as a new one");
        addDial (noiseDial,    "NOISE WIDTH",  mProcessor->analyzerNoiseWidthHzValue,  100.0, 8000.0, 10.0, " Hz", 0.35,
                 "Bandwidth of the noisy leftovers tracked around each partial");
        addDial (fundDial,     "FUNDAMENTAL",  mProcessor->analyzerFundamentalHzValue,   0.0, 2000.0, 1.0, " Hz", 0.35,
                 "Reference pitch guiding Clean Partials and the partials view (0 = detect automatically)");

        srcVolFader = std::make_unique<SquareFader> (mProcessor->previewSrcVolDbValue,
                                                         "SOURCE", -40.0, 6.0, 0.5, " dB");
        srcVolFader->setTooltip ("Monitor volume for source preview");
        srcVolFader->setVerticalValue (true);
        srcVolFader->setTrackInsetU (4.0f);
        srcVolFader->setCaptionScale (0.7f);
        addAndMakeVisible (srcVolFader.get());

        rsynVolFader = std::make_unique<SquareFader> (mProcessor->previewRsynVolDbValue,
                                                          "RESYNTHESIS", -40.0, 6.0, 0.5, " dB");
        rsynVolFader->setTooltip ("Monitor volume for resynthesis preview");
        rsynVolFader->setVerticalValue (true);
        rsynVolFader->setTrackInsetU (4.0f);
        rsynVolFader->setCaptionScale (0.7f);
        addAndMakeVisible (rsynVolFader.get());

        addDial (onsetKnob, "ONSET SENS",
                 mProcessor->analyzerOnsetSensitivityValue,
                 boreal::analyzer::Settings::kOnsetSensMin,
                 boreal::analyzer::Settings::kOnsetSensMax, 0.005, " x", 1.0,
                 "Onset sensitivity -- higher finds more onsets (live update, no re-analysis needed)");

        onsetKnob->onValueChangeInternal = [this]
        {
            onsetDirty = true;
            lastOnsetEditMs = juce::Time::getMillisecondCounter();
        };

        fundDial->onValueChangeInternal = [this]
        { partialsView.setFundamentalHz (mProcessor->getAnalyzerFundamentalHz()); };
        partialsView.setFundamentalHz (mProcessor->getAnalyzerFundamentalHz());

        cleanPartialsButton.setClickingTogglesState (true);
        cleanPartialsButton.setToggleState (mProcessor->getAnalyzerCleanPartials(),
                                            juce::NotificationType::dontSendNotification);
        cleanPartialsButton.setTooltip ("Clean partials: channelize + distill around the detected "
                                        "fundamental -- removes octave errors and cross-talk "
                                        "(applies on the next analysis)");
        cleanPartialsButton.onClick = [this]
        { mProcessor->analyzerCleanPartialsValue = cleanPartialsButton.getToggleState(); };
        addAndMakeVisible (cleanPartialsButton);

        clearButton.setTooltip ("Remove the source audio, partials and resynthesis");
        clearButton.onClick = [this]
        {
            auto doClear = [this]
            {
                mProcessor->clearAnalyzerSession();
                sourceView.setWaveform (nullptr, 0.0);
                sourceView.setIntervalInternal (0.0, 1.0);
                partialsView.setData (nullptr);
                resynthView.setWaveform (nullptr, 0.0);
                infoLabel.setText ("> CLICK SOURCE OR PRESS OPEN", juce::NotificationType::dontSendNotification);
            };
            if (! mProcessor->getConfirmDestructive())
            {
                doClear();
                return;
            }
            juce::AlertWindow::showOkCancelBox (
                juce::MessageBoxIconType::QuestionIcon,
                "Clear analyzer?",
                "Remove the source audio, partials and resynthesis?",
                "Clear", "Cancel", this,
                juce::ModalCallbackFunction::create ([doClear] (int result)
                {
                    if (result != 0)
                        doClear();
                }));
        };
        addAndMakeVisible (clearButton);

        vizExpandButton.setTooltip ("Expand visualization -- hide the parameters section so waveforms and partials fill the window");
        vizExpandButton.onClick = [this] { setViewMode (ViewMode::VizFull); };
        addAndMakeVisible (vizExpandButton);

        paramsExpandButton.setTooltip ("Expand parameters -- hide the visualization section so parameters fill the window");
        paramsExpandButton.onClick = [this] { setViewMode (ViewMode::ParamsFull); };
        addAndMakeVisible (paramsExpandButton);

        auto wireTextButton = [this] (juce::TextButton& b, const char* name, const char* tip, std::function<void()> fn)
        {
            b.setButtonText (name);
            b.setTooltip (tip);
            b.setLookAndFeel (&smallButtonLnf);
            b.onClick = std::move (fn);
            addAndMakeVisible (b);
        };

        wireTextButton (openButton, "OPEN AUDIO", "Open an audio file for analysis", [this] { chooseOpen(); });
        wireTextButton (analyzeButton, "ANALYZE", "Analyze the loaded source into partials", [this] { mProcessor->analyzeScratch(); });
        wireTextButton (synthButton, "SYNTHESIZE", "Resynthesize audio from the current partials", [this] { mProcessor->synthesizeScratch(); });
        playSrcButton.onClick = [this] { togglePreview (BorealAudioProcessor::previewSource, playSrcButton, "PLAY SOURCE"); };
        wireTextButton (playSrcButton, "PLAY SOURCE", "Play or stop the source audio", playSrcButton.onClick);
        wireTextButton (exportSdifButton, "EXPORT SDIF", "Export the analysis as an SDIF file", [this] { chooseExportSdif(); });

        playRsnButton.onClick = [this] { togglePreview (BorealAudioProcessor::previewResynth, playRsnButton, "PLAY RESYNTH"); };
        wireTextButton (playRsnButton, "PLAY RESYNTH", "Play or stop the resynthesis", playRsnButton.onClick);
        wireTextButton (exportWavButton, "EXPORT WAV", "Export the resynthesis as a WAV file", [this] { chooseExportWav(); });

        applyActionAvailability();
        syncViewMode();

        infoLabel.setJustificationType (juce::Justification::centredLeft);
        infoLabel.setFont (CustomLookAndFeel::makeFont (14.0f));
        infoLabel.setColour (juce::Label::textColourId, GUI::Color::Logo.withAlpha (0.75f));
        infoLabel.setText ("> CLICK SOURCE OR PRESS OPEN", juce::NotificationType::dontSendNotification);
        addAndMakeVisible (infoLabel);

        startTimer (timerFast, 33);
        startTimer (timerSlow, 150);
    }

    ~AnalyzerWindow() override
    {
        stopTimer (timerFast);
        stopTimer (timerSlow);
        for (auto* b : { &openButton, &analyzeButton, &synthButton,
                         &playSrcButton, &exportSdifButton, &playRsnButton, &exportWavButton })
            b->setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        drawCard (g, card1Rect, ">> VISUALIZATION");
        drawCard (g, card2Rect, viewMode == ViewMode::ParamsFull ? ">> ANALYSIS" : ">> PARAMETERS");
        drawInnerTitle (g, paramsRect, viewMode == ViewMode::ParamsFull ? ">> PARAMETERS" : ">> ANALYSIS");
        drawInnerTitle (g, volumeRect, ">> VOLUME");
        drawInnerTitle (g, actionsRect, ">> ACTIONS");

        if (isDragOver)
        {
            g.setColour (GUI::Color::Accent.withAlpha (0.60f));
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (3.0f), 6.0f, 2.0f);
        }
    }

    void resized() override
    {
        const float zs = BorealZoom::uiScale;
        const int pad = juce::roundToInt (8.0f * zs);
        const int gapC = juce::roundToInt (8.0f * zs);
        const int titleH = juce::roundToInt (22.0f * zs);
        auto area = getLocalBounds().reduced ((int) (GUI::Layout::MainMargin * zs));

        infoLabel.setBounds (area.removeFromBottom (juce::roundToInt (16.0f * zs)));
        area.removeFromBottom (gapC);

        auto card1 = area;
        auto card2 = area;
        if (viewMode == ViewMode::VizFull)
        {
            card2 = juce::Rectangle<int>();
        }
        else if (viewMode == ViewMode::ParamsFull)
        {
            card1 = juce::Rectangle<int>();
        }
        else
        {
            const int card2H = (int) (area.getHeight() * 0.38f);
            card2 = area.removeFromBottom (card2H);
            area.removeFromBottom (gapC);
            card1 = area;
        }

        card1Rect = card1;
        card2Rect = card2;

        const auto headerCard = (viewMode == ViewMode::ParamsFull) ? card2 : card1;
        const int btn = juce::roundToInt (18.0f * zs);
        const int btnGap = juce::roundToInt (6.0f * zs);
        const int rightPad = juce::roundToInt (10.0f * zs);
        const int hdrY = headerCard.getY() + pad;
        int bx = headerCard.getRight() - rightPad - btn;
        clearButton.setBounds (bx, hdrY, btn, btn);
        bx -= btn + btnGap;
        paramsExpandButton.setBounds (bx, hdrY, btn, btn);
        bx -= btn + btnGap;
        vizExpandButton.setBounds (bx, hdrY, btn, btn);

        if (viewMode != ViewMode::ParamsFull)
        {
            auto v = card1.reduced (pad);
            v.removeFromTop (titleH);
            sourceView.setBounds (v.removeFromTop (juce::roundToInt (64.0f * zs)));
            v.removeFromTop (gapC);
            partialsView.setBounds (v.removeFromTop (juce::jmax (0, v.getHeight() - juce::roundToInt (64.0f * zs) - gapC)));
            v.removeFromTop (gapC);
            resynthView.setBounds (v);
        }

        if (viewMode != ViewMode::VizFull)
        {
            auto c2 = card2.reduced (pad);
            c2.removeFromTop (titleH);
            if (viewMode == ViewMode::ParamsFull)
            {
                const int specMinH = juce::roundToInt (64.0f * zs);
                const int paramsH = juce::jmax (0, juce::jmin (juce::roundToInt (230.0f * zs),
                                                               c2.getHeight() - gapC - specMinH));
                auto paramsArea = c2.removeFromBottom (paramsH);
                c2.removeFromBottom (juce::roundToInt (4.0f * zs));
                partialsView.setBounds (c2);
                c2 = paramsArea;
            }
            else if (c2.getHeight() > juce::roundToInt (230.0f * zs))
            {
                c2 = c2.withSizeKeepingCentre (c2.getWidth(), juce::roundToInt (230.0f * zs));
            }
            auto actions = c2.removeFromRight ((int) (c2.getWidth() * 0.30f));
            c2.removeFromRight (gapC);
            const int volW = juce::jmin (juce::roundToInt (140.0f * zs), c2.getWidth() / 2);
            auto volume = c2.removeFromRight (volW);
            c2.removeFromRight (gapC);
            auto analysis = c2;

            paramsRect = analysis;
            volumeRect = volume;
            actionsRect = actions;

            auto ab = analysis;
            ab.removeFromLeft (juce::roundToInt (12.0f * zs));
            ab.removeFromRight (juce::roundToInt (12.0f * zs));
            ab.removeFromTop (viewMode == ViewMode::ParamsFull ? juce::roundToInt (4.0f * zs)
                                                              : juce::roundToInt (8.0f * zs));
            ab.removeFromBottom (juce::roundToInt (12.0f * zs));
            ab.removeFromTop (juce::roundToInt (17.0f * zs));
            layoutDialBank (ab);

            auto vb = volume;
            vb.removeFromLeft (juce::roundToInt (12.0f * zs));
            vb.removeFromRight (juce::roundToInt (12.0f * zs));
            vb.removeFromTop (juce::roundToInt (8.0f * zs));
            vb.removeFromBottom (juce::roundToInt (12.0f * zs));
            vb.removeFromTop (juce::roundToInt (17.0f * zs));
            const int faderW = vb.getWidth() / 2;
            srcVolFader->setBounds (vb.removeFromLeft (faderW));
            rsynVolFader->setBounds (vb);

            auto ac = actions;
            ac.removeFromLeft (juce::roundToInt (12.0f * zs));
            ac.removeFromRight (juce::roundToInt (12.0f * zs));
            ac.removeFromTop (juce::roundToInt (8.0f * zs));
            ac.removeFromBottom (juce::roundToInt (12.0f * zs));
            ac.removeFromTop (juce::roundToInt (17.0f * zs));
            layoutButtonGrid (ac);
        }
        else
        {
            paramsRect = juce::Rectangle<int>();
            volumeRect = juce::Rectangle<int>();
            actionsRect = juce::Rectangle<int>();
        }
    }

    void drawCard (juce::Graphics& g, const juce::Rectangle<int>& card, const juce::String& title)
    {
        if (card.getWidth() <= 0 || card.getHeight() <= 0)
            return;

        const float zs = BorealZoom::uiScale;
        g.setColour (GUI::Color::CardDark);
        g.fillRoundedRectangle (GUI::Paint::insetCardBounds (card.toFloat()), 4.0f * zs);
        GUI::Paint::drawCardOutline (g, card.toFloat(), 4.0f * zs);

        g.setColour (GUI::Color::KeyDown);
        g.setFont (CustomLookAndFeel::makeFont (18.0f));
        g.drawText (title, card.reduced (juce::roundToInt (12.0f * zs), 0)
                                .withTrimmedTop (juce::roundToInt (4.0f * zs))
                                .withHeight (juce::roundToInt (24.0f * zs)),
                    juce::Justification::topLeft, false);
    }

    void drawInnerTitle (juce::Graphics& g, const juce::Rectangle<int>& inner, const juce::String& title)
    {
        if (inner.getWidth() <= 0 || inner.getHeight() <= 0)
            return;

        const float zs = BorealZoom::uiScale;
        GUI::Paint::drawCardOutline (g, inner.toFloat(), GUI::Layout::InnerCardCorner);
        g.setColour (GUI::Color::KeyDown);
        const float titleSize = (viewMode == ViewMode::ParamsFull) ? 16.0f : 13.0f;
        g.setFont (CustomLookAndFeel::makeFont (titleSize));
        g.drawText (title, inner.reduced (juce::roundToInt (8.0f * zs), 0)
                                .withTrimmedTop (juce::roundToInt (4.0f * zs))
                                .withHeight (juce::roundToInt (titleSize * zs)),
                    juce::Justification::topLeft, false);
    }

    static bool isSupportedDropFile (const juce::File& f)
    {
        const juce::String ext = f.getFileExtension().toLowerCase();
        return ext == ".wav" || ext == ".wave" || ext == ".aif" || ext == ".aiff"
            || ext == ".mp3" || ext == ".flac" || ext == ".ogg" || ext == ".sdif";
    }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        if (files.isEmpty())
            return true;
        for (auto& f : files)
            if (isSupportedDropFile (juce::File (f)))
                return true;
        return false;
    }

    void fileDragEnter (const juce::StringArray&, int, int) override
    {
        isDragOver = true;
        repaint();
    }

    void fileDragExit (const juce::StringArray&) override
    {
        isDragOver = false;
        repaint();
    }

    void filesDropped (const juce::StringArray& files, int, int) override
    {
        isDragOver = false;
        for (auto& f : files)
        {
            if (isSupportedDropFile (juce::File (f)))
            {
                handleDroppedAudioFile (juce::File (f));
                break;
            }
        }
        repaint();
    }

    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details) override
    {
        const juce::String path = details.description.toString();
        return path != "directory" && isSupportedDropFile (juce::File (path));
    }

    void itemDragEnter (const juce::DragAndDropTarget::SourceDetails&) override
    {
        isDragOver = true;
        repaint();
    }

    void itemDragExit (const juce::DragAndDropTarget::SourceDetails&) override
    {
        isDragOver = false;
        repaint();
    }

    void itemDropped (const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override
    {
        isDragOver = false;
        const juce::String path = dragSourceDetails.description.toString();
        if (path != "directory" && isSupportedDropFile (juce::File (path)))
            handleDroppedAudioFile (juce::File (path));
        repaint();
    }

    void handleDroppedAudioFile (const juce::File& f)
    {
        if (f.getFileExtension().equalsIgnoreCase (".sdif"))
        {
            infoLabel.setText ("> .sdif files load into slots, not the analyzer -- open audio instead",
                               juce::NotificationType::dontSendNotification);
            return;
        }

        const juce::String err = mProcessor->openAnalyzerSource (f);
        if (err.isNotEmpty())
            infoLabel.setText ("> " + err, juce::NotificationType::dontSendNotification);
        else
        {
            sourceView.setIntervalInternal (0.0, 1.0);
            refreshSessionViews();
        }
    }

private:
    static constexpr int timerFast = 1;
    static constexpr int timerSlow = 2;

    struct SmallButtonLookAndFeel : public CustomLookAndFeel
    {
        juce::Font getTextButtonFont (juce::TextButton&, int) override
        {
            return CustomLookAndFeel::makeFont (17.0f);
        }
    };

    struct HeaderIconButton : public juce::ToggleButton
    {
        enum class Kind { Monitor, Faders, Close };

        explicit HeaderIconButton (Kind k) : kind (k) {}

        void paint (juce::Graphics& g) override
        {
            getLookAndFeel().drawButtonBackground (g, *this, findColour (juce::TextButton::buttonColourId),
                                                   isMouseOver(), isDown());

            const auto bounds = getLocalBounds().toFloat();
            const float cx = bounds.getCentreX();
            const float cy = bounds.getCentreY();
            const float u = BorealZoom::uiScale;
            const bool toggledOn = getToggleState();

            g.setColour (toggledOn || isMouseOver() ? BorealColors::textPrimary : BorealColors::textMid);

            if (kind == Kind::Monitor)
            {
                g.drawRoundedRectangle (cx - 6.2f * u, cy - 5.6f * u, 12.4f * u, 7.9f * u,
                                        1.0f * u, 1.2f * u);
                g.drawLine (cx, cy + 2.3f * u, cx, cy + 4.7f * u, 1.3f * u);
                g.drawLine (cx - 3.1f * u, cy + 4.7f * u, cx + 3.1f * u, cy + 4.7f * u, 1.3f * u);
            }
            else if (kind == Kind::Faders)
            {
                const float lineH = 1.2f * u;
                const float halfW = 5.8f * u;
                const float knob = 2.5f * u;
                const float rows[3] = { cy - 3.6f * u, cy, cy + 3.6f * u };
                const float knobX[3] = { cx - 2.2f * u, cx + 1.8f * u, cx - 0.4f * u };
                for (int i = 0; i < 3; ++i)
                {
                    g.fillRect (cx - halfW, rows[i] - lineH * 0.5f, halfW * 2.0f, lineH);
                    g.fillRect (knobX[i] - knob * 0.5f, rows[i] - knob * 0.5f, knob, knob);
                }
            }
            else
            {
                const float arm = 4.0f * u;
                g.drawLine (cx - arm, cy - arm, cx + arm, cy + arm, 1.7f * u);
                g.drawLine (cx + arm, cy - arm, cx - arm, cy + arm, 1.7f * u);
            }
        }

    private:
        Kind kind;
    };

    struct EraserToggleButton : public juce::ToggleButton
    {
        void paint (juce::Graphics& g) override
        {
            const float u = BorealZoom::uiScale;
            auto bounds = getLocalBounds().toFloat();

            const float labelH = 17.0f * u;
            auto body = bounds;
            auto labelArea = body.removeFromBottom (labelH);

            const float side = juce::jmin (body.getWidth(), body.getHeight());
            if (side <= 0.0f)
                return;
            const auto sq = juce::Rectangle<float> (body.getCentreX() - side * 0.5f,
                                                    body.getCentreY() - side * 0.5f,
                                                    side, side).reduced (2.0f * u);
            const float corner = 3.0f * u;
            const bool toggledOn = getToggleState();

            g.setColour (GUI::Color::Background);
            g.fillRoundedRectangle (sq, corner);
            if (toggledOn || isMouseOverOrDragging())
            {
                g.setColour (GUI::Color::Logo.withAlpha (
                    toggledOn ? 0.28f : 0.38f));
                g.fillRoundedRectangle (sq, corner);
            }
            g.setColour (GUI::Color::Logo.withAlpha (0.60f));
            g.drawRoundedRectangle (sq, corner, 1.0f * u);

            const float gu = sq.getWidth() / 26.0f * u;
            const float cx = sq.getCentreX();
            const float cy = sq.getCentreY();

            g.setColour (toggledOn || isMouseOver() ? BorealColors::textPrimary : BorealColors::textMid);

            g.saveState();
            g.addTransform (juce::AffineTransform::rotation (
                -juce::MathConstants<float>::pi / 6.0f, cx, cy));
            g.drawRoundedRectangle (cx - 6.5f * gu, cy - 3.5f * gu, 13.0f * gu, 7.0f * gu,
                                    1.5f * gu, 1.4f * gu);
            g.drawLine (cx + 2.5f * gu, cy - 3.5f * gu, cx + 2.5f * gu, cy + 3.5f * gu, 1.4f * gu);
            g.restoreState();

            g.setColour (GUI::Color::Logo.withAlpha (0.75f));
            g.setFont (CustomLookAndFeel::makeFont (14.0f * fontScale));
            g.drawText ("CLEAN", labelArea.translated (0.0f, 2.0f * u),
                        juce::Justification::centred, false);
        }

        void setFontScale (float s) noexcept
        {
            if (fontScale != s)
            {
                fontScale = s;
                repaint();
            }
        }

    private:
        float fontScale = 1.0f;
    };

    void layoutDialBank (juce::Rectangle<int> area)
    {
        juce::Component* cells[] = {
            resDial.get(), ampFloorDial.get(), widthDial.get(),
            loCutDial.get(), hiCutDial.get(), driftDial.get(),
            noiseDial.get(), fundDial.get(), onsetKnob.get(),
            &cleanPartialsButton
        };

        const float zs = BorealZoom::uiScale;
        const int cols = 5, rows = 2;
        const int cellW = area.getWidth() / cols;
        const int rowGap = juce::roundToInt (8.0f * zs);
        const int cellH = (area.getHeight() - rowGap) / rows;
        const int inset = juce::roundToInt (2.0f * zs);
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
                if (auto* cell = cells[(size_t) (r * cols + c)])
                {
                    auto cellRect = juce::Rectangle<int> (
                        area.getX() + c * cellW, area.getY() + r * (cellH + rowGap),
                        cellW, cellH).reduced (inset);
                    cell->setBounds (cellRect);
                }
    }

    void layoutButtonGrid (juce::Rectangle<int> area)
    {
        juce::TextButton* left[3] =
        {
            &openButton, &analyzeButton, &synthButton,
        };
        juce::TextButton* right[4] =
        {
            &playSrcButton, &playRsnButton, &exportSdifButton, &exportWavButton,
        };

        const float zs = BorealZoom::uiScale;
        const int gapG = juce::roundToInt (8.0f * zs);
        const int colW = (area.getWidth() - gapG) / 2;

        auto layoutColumn = [&] (juce::TextButton** buttons, int count, int x)
        {
            const int total = area.getHeight();
            for (int i = 0; i < count; ++i)
            {
                const int top = area.getY() + (i * (total + gapG)) / count;
                const int bottom = (i + 1 == count) ? area.getBottom()
                    : area.getY() + (((i + 1) * (total + gapG)) / count) - gapG;
                buttons[i]->setBounds (x, top, colW, juce::jmax (1, bottom - top));
            }
        };

        layoutColumn (left, 3, area.getX());
        layoutColumn (right, 4, area.getX() + colW + gapG);
    }

    void applyActionAvailability()
    {
        const bool hasSource = mProcessor->hasAnalyzerSource();
        const auto frames = mProcessor->getAnalyzerFrames();
        const bool hasFrames = frames != nullptr && ! frames->times.empty();
        const bool hasResynth = mProcessor->getAnalyzerResynthRate() > 0.0
                                && ! mProcessor->getAnalyzerResynth().empty();
        const bool idle = mProcessor->getAnalyzerJobKind() < 0;

        setActionAvailable (openButton, true);
        setActionAvailable (analyzeButton, hasSource && idle);
        setActionAvailable (playSrcButton, hasSource && previewFreeFor (BorealAudioProcessor::previewSource));
        setActionAvailable (synthButton, hasFrames && idle);
        setActionAvailable (playRsnButton, hasResynth && previewFreeFor (BorealAudioProcessor::previewResynth));
        setActionAvailable (exportSdifButton, hasFrames && idle);
        setActionAvailable (exportWavButton, hasResynth);
    }

    bool previewFreeFor (int mode) const
    {
        return ! mProcessor->previewPlaying() || mProcessor->previewMode() == mode;
    }

    static void setActionAvailable (juce::TextButton& b, bool available)
    {
        if (b.isEnabled() != available)
        {
            b.setEnabled (available);
            b.setAlpha (available ? 1.0f : 0.35f);
        }
    }

    void togglePreview (int mode, juce::TextButton& button, const char* playText)
    {
        if (mProcessor->previewPlaying() && mProcessor->previewMode() == mode)
        {
            mProcessor->previewStop();
            button.setButtonText (playText);
            infoLabel.setText ("> playback stopped", juce::NotificationType::dontSendNotification);
            applyActionAvailability();
            return;
        }

        mProcessor->previewStop();

        if (! mProcessor->previewPlay (mode))
        {
            infoLabel.setText (mode == BorealAudioProcessor::previewResynth
                                   ? "> nothing synthesized yet -- press SYNTHESIZE first"
                                   : "> no source loaded",
                               juce::NotificationType::dontSendNotification);
            applyActionAvailability();
            return;
        }
        button.setButtonText ("STOP");
        applyActionAvailability();

        const double volDb = (mode == BorealAudioProcessor::previewSource)
                                 ? (double) mProcessor->previewSrcVolDbValue.getValue()
                                 : (double) mProcessor->previewRsynVolDbValue.getValue();
        infoLabel.setText (juce::String ("> playing ")
                               + (mode == BorealAudioProcessor::previewSource ? "SOURCE" : "RESYNTHESIS")
                               + "..."
                               + (volDb < -30.0
                                      ? "  -- note: volume dial is at " + juce::String (volDb, 1) + " dB"
                                      : juce::String()),
                           juce::NotificationType::dontSendNotification);
    }

    void chooseOpen()
    {
        openChooser = std::make_unique<juce::FileChooser> (
            "Open audio for analysis", juce::File (getDefaultCollectionsDirectory()),
            "*.wav;*.wave;*.aif;*.aiff;*.mp3;*.flac;*.ogg");
        openChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      const auto f = fc.getResult();
                                      if (! f.existsAsFile())
                                          return;
                                      const juce::String err = mProcessor->openAnalyzerSource (f);
                                      if (err.isNotEmpty())
                                          infoLabel.setText ("> " + err, juce::NotificationType::dontSendNotification);
                                      else
                                      {
                                          sourceView.setIntervalInternal (0.0, 1.0);
                                          refreshSessionViews();
                                      }
                                  });
    }

    void chooseExportSdif()
    {
        exportSdifChooser = std::make_unique<juce::FileChooser> ("Export SDIF + attack sidecar pair to folder",
                                                                 getDefaultCollectionsDirectory());
        exportSdifChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                        [this] (const juce::FileChooser& fc)
                                        {
                                            const auto dir = fc.getResult();
                                            if (dir.isDirectory())
                                                mProcessor->exportScratchSdifAsync (dir);
                                        });
    }

    void chooseExportWav()
    {
        exportWavChooser = std::make_unique<juce::FileChooser> ("Export resynthesis as WAV",
                                                                getDefaultCollectionsDirectory(), "*.wav");
        exportWavChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                                       [this] (const juce::FileChooser& fc)
                                       {
                                           const auto f = fc.getResult();
                                           if (! mProcessor->exportScratchWav (f))
                                               infoLabel.setText ("> nothing synthesized yet", juce::NotificationType::dontSendNotification);
                                       });
    }

    void timerCallback (int id) override
    {
        if (id == timerFast)
        {
            const bool playing = mProcessor->previewPlaying();
            if (playing)
            {
                sourceView.repaint();
                resynthView.repaint();
            }

            if (wasPlaying && ! playing)
            {
                infoLabel.setText ("> playback finished", juce::NotificationType::dontSendNotification);
                playSrcButton.setButtonText ("PLAY SOURCE");
                playRsnButton.setButtonText ("PLAY RESYNTH");
                applyActionAvailability();
            }
            wasPlaying = playing;

            const int kind = mProcessor->getAnalyzerJobKind();
            if (kind >= 0)
            {
                const int pct = (int) std::round (100.0f * mProcessor->getAnalyzerJobProgress());
                const juce::String what = kind == 0 ? "ANALYZING" : "SYNTHESIZING";
                infoLabel.setText ("> " + what + "... "
                                       + juce::String (pct) + "% "
                                       + mProcessor->getAnalyzerJobStage().toUpperCase(),
                                   juce::NotificationType::dontSendNotification);
                return;
            }
            if (playing)
            {
                double pos = mProcessor->previewPlayheadSeconds();
                const bool src = mProcessor->previewMode() == BorealAudioProcessor::previewSource;
                double dur = 0.0;
                if (src)
                    dur = mProcessor->getAnalyzerSourceDurationSec();
                else if (mProcessor->getAnalyzerResynthRate() > 0.0)
                    dur = (double) mProcessor->getAnalyzerResynth().size()
                          / mProcessor->getAnalyzerResynthRate();

                pos = juce::jlimit (0.0, dur > 0.0 ? dur : pos, pos);
                infoLabel.setText (juce::String ("> playing ")
                                       + (src ? "SOURCE" : "RESYNTH")
                                       + "... " + juce::String (pos, 2) + "s / "
                                       + juce::String (dur, 2) + "s",
                                   juce::NotificationType::dontSendNotification);
                return;
            }
            composeIdleInfoLine();
            return;
        }

        if (onsetDirty && juce::Time::getMillisecondCounter() - lastOnsetEditMs > 150)
        {
            onsetDirty = false;
            if (mProcessor->getAnalyzerJobKind() < 0)
            {
                mProcessor->refreshScratchOnsets();
                lastFramesCount = -1;
            }
        }

        refreshSessionViews();

        clearButton.setVisible (mProcessor->hasAnalyzerSource());
        applyActionAvailability();
        const bool cp = mProcessor->getAnalyzerCleanPartials();
        if (cleanPartialsButton.getToggleState() != cp)
            cleanPartialsButton.setToggleState (cp, juce::NotificationType::dontSendNotification);

        if (! mProcessor->previewPlaying())
        {
            if (playSrcButton.getButtonText() == "STOP") playSrcButton.setButtonText ("PLAY SOURCE");
            if (playRsnButton.getButtonText() == "STOP") playRsnButton.setButtonText ("PLAY RESYNTH");
        }
    }

    void refreshSessionViews()
    {
        const int srcGen = mProcessor->getAnalyzerSourceGeneration();
        if (srcGen != lastSourceGen)
        {
            lastSourceGen = srcGen;
            lastFramesCount = -1;
            lastResynthSize = -1;

            if (srcGen > 0)
                sourceView.setWaveform (mProcessor->getAnalyzerSourceBuffer(),
                                        mProcessor->getAnalyzerSampleRate());
            else
                sourceView.setWaveform (nullptr, 0.0);

            partialsView.setData (nullptr);
        }

        const auto frames = mProcessor->getAnalyzerFrames();
        const int frameCount = frames != nullptr ? (int) frames->times.size() : -1;
        const int rsSize = (int) mProcessor->getAnalyzerResynth().size();

        if (frameCount == lastFramesCount && rsSize == lastResynthSize)
            return;
        lastFramesCount = frameCount;
        lastResynthSize = rsSize;

        partialsView.setData (frames);
        partialsView.setFundamentalHz (mProcessor->getAnalyzerFundamentalHz());

        const auto& rs = mProcessor->getAnalyzerResynth();
        auto resynthBuf = std::make_shared<std::vector<float>> (rs.size());
        for (size_t i = 0; i < resynthBuf->size(); ++i)
            (*resynthBuf)[i] = (float) rs[i];
        resynthView.setWaveform (std::move (resynthBuf), mProcessor->getAnalyzerResynthRate());
    }

    void composeIdleInfoLine()
    {
        if (! mProcessor->hasAnalyzerSource())
        {
            infoLabel.setText ("> CLICK SOURCE OR PRESS OPEN", juce::NotificationType::dontSendNotification);
            return;
        }

        const juce::String name = mProcessor->getAnalyzerSourceName();
        const double dur = mProcessor->getAnalyzerSourceDurationSec();
        const auto frames = mProcessor->getAnalyzerFrames();

        juce::String line = name + "  [" + juce::String (dur, 2) + "s @ "
                            + juce::String ((int) mProcessor->getAnalyzerSampleRate()) + " Hz]";
        if (frames != nullptr && ! frames->times.empty())
        {
            line << "   partials: " << (frames->offsets.size() - 1)
                 << "   max freq: " << (int) frames->maxFrequencyHz << " Hz"
                 << "   max active: " << frames->maxActiveCount
                 << "   onsets: " << frames->onsets.size();
        }
        else
        {
            line << "   (not analyzed yet)";
        }
        infoLabel.setText ("> " + line, juce::NotificationType::dontSendNotification);
    }

    enum class ViewMode { Split, VizFull, ParamsFull };

    void setViewMode (ViewMode m)
    {
        if (m == viewMode)
            m = ViewMode::Split;
        viewMode = m;
        syncViewMode();
        resized();
    }

    void syncViewMode()
    {
        vizExpandButton.setToggleState (viewMode == ViewMode::VizFull,
                                        juce::NotificationType::dontSendNotification);
        paramsExpandButton.setToggleState (viewMode == ViewMode::ParamsFull,
                                           juce::NotificationType::dontSendNotification);
        setKnobFontScale (viewMode == ViewMode::ParamsFull ? 1.25f : 1.0f);
        applyViewVisibility();
    }

    void setKnobFontScale (float s)
    {
        for (auto* k : { resDial.get(), ampFloorDial.get(), widthDial.get(),
                         loCutDial.get(), hiCutDial.get(), driftDial.get(),
                         noiseDial.get(), fundDial.get(), onsetKnob.get() })
            if (k != nullptr)
                k->setFontScale (s);
        if (srcVolFader != nullptr)
            srcVolFader->setFontScale (2.0f);
        if (rsynVolFader != nullptr)
            rsynVolFader->setFontScale (2.0f);
        cleanPartialsButton.setFontScale (s);
    }

    void applyViewVisibility()
    {
        const bool showViz = viewMode != ViewMode::ParamsFull;
        const bool showParams = viewMode != ViewMode::VizFull;
        sourceView.setVisible (showViz);
        partialsView.setVisible (true);
        resynthView.setVisible (showViz);

        for (auto* k : { resDial.get(), ampFloorDial.get(), widthDial.get(),
                         loCutDial.get(), hiCutDial.get(), driftDial.get(),
                         noiseDial.get(), fundDial.get(), onsetKnob.get() })
            if (k != nullptr)
                k->setVisible (showParams);
        if (srcVolFader != nullptr)
            srcVolFader->setVisible (showParams);
        if (rsynVolFader != nullptr)
            rsynVolFader->setVisible (showParams);
        cleanPartialsButton.setVisible (showParams);
        for (auto* b : { &openButton, &analyzeButton, &synthButton,
                         &playSrcButton, &exportSdifButton, &playRsnButton, &exportWavButton })
            b->setVisible (showParams);
    }

    BorealAudioProcessor* mProcessor;

    bool isDragOver = false;
    ViewMode viewMode = ViewMode::ParamsFull;
    bool onsetDirty = false;
    uint32_t lastOnsetEditMs = 0;

    juce::Rectangle<int> card1Rect, card2Rect, paramsRect, volumeRect, actionsRect;

    WaveformView sourceView;
    PartialsView partialsView;
    WaveformView resynthView;
    juce::Label infoLabel;

    int lastSourceGen = -1;
    int lastFramesCount = -1;
    int lastResynthSize = -1;
    bool wasPlaying = false;

    std::unique_ptr<SquareKnob> resDial, ampFloorDial, widthDial, loCutDial, hiCutDial;
    std::unique_ptr<SquareKnob> driftDial, noiseDial, fundDial;
    std::unique_ptr<SquareFader> srcVolFader, rsynVolFader;

    EraserToggleButton cleanPartialsButton;
    std::unique_ptr<SquareKnob> onsetKnob;

    SmallButtonLookAndFeel smallButtonLnf;

    juce::TextButton openButton, analyzeButton, synthButton;
    juce::TextButton playSrcButton, playRsnButton, exportSdifButton, exportWavButton;
    HeaderIconButton clearButton { HeaderIconButton::Kind::Close };
    HeaderIconButton vizExpandButton { HeaderIconButton::Kind::Monitor };
    HeaderIconButton paramsExpandButton { HeaderIconButton::Kind::Faders };

    std::unique_ptr<juce::FileChooser> openChooser;
    std::unique_ptr<juce::FileChooser> exportSdifChooser;
    std::unique_ptr<juce::FileChooser> exportWavChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalyzerWindow)
};
