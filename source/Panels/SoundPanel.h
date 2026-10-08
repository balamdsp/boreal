#pragma once

#include <JuceHeader.h>

#include "../Helpers/CustomLookAndFeel.h"
#include "../Helpers/InterfaceDefines.h"
#include "../Helpers/BorealConstants.h"
#include "../Helpers/BorealParameters.h"
#include "../Components/Slider.h"
#include "../Components/SquareKnob.h"
#include "../Components/TransportIconButton.h"
#include "../PluginProcessor.h"

class ViewFlipButton : public TextButton
{
public:
    using TextButton::TextButton;

    void setCursorView (bool inCursorView) noexcept
    {
        if (cursorView != inCursorView)
        {
            cursorView = inCursorView;
            repaint();
        }
    }

    void paint (Graphics& g) override
    {
        getLookAndFeel().drawButtonBackground (g, *this, findColour (buttonColourId), isMouseOver(), isDown());

        const auto bounds = getLocalBounds().toFloat();
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float u = BorealZoom::uiScale;

        g.setColour (isMouseOver() ? BorealColors::textPrimary : BorealColors::textMid);

        if (cursorView)
        {
            const float lineW = 2.0f * u;
            const float s = 2.6f * u;
            const float top = cy - 5.0f * u + s * 0.5f;
            const float bottom = cy + 5.0f * u + s * 0.5f;
            g.fillRect (cx - lineW * 0.5f, top, lineW, bottom - top);
            juce::Path tri;
            tri.addTriangle (cx - s, top - s, cx + s, top - s, cx, top + s * 0.4f);
            g.fillPath (tri);
        }
        else
        {
            const float lineH = 1.5f * u;
            const float halfW = 7.0f * u;
            const float knob = 3.0f * u;
            const float rows[3] = { cy - 4.5f * u, cy, cy + 4.5f * u };
            const float knobX[3] = { cx - 3.0f * u, cx + 2.0f * u, cx - 0.5f * u };
            for (int i = 0; i < 3; ++i)
            {
                g.fillRect (cx - halfW, rows[i] - lineH * 0.5f, halfW * 2.0f, lineH);
                g.fillRect (knobX[i] - knob * 0.5f, rows[i] - knob * 0.5f, knob, knob);
            }
        }
    }

private:
    bool cursorView = false;
};

class ReverseToggleButton : public TextButton
{
public:
    using TextButton::TextButton;

    void paint (Graphics& g) override
    {
        getLookAndFeel().drawButtonBackground (g, *this, findColour (buttonColourId), isMouseOver(), isDown());

        const auto bounds = getLocalBounds().toFloat();
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float u = BorealZoom::uiScale;

        g.setColour (getToggleState() || isMouseOver() ? BorealColors::textPrimary : BorealColors::textMid);

        const float halfH = 5.0f * u;
        const float barW = 2.0f * u;
        const float barX = cx - 5.5f * u;
        g.fillRect (barX, cy - halfH, barW, halfH * 2.0f);

        juce::Path tri;
        tri.addTriangle (cx - 2.5f * u, cy,
                         cx + 4.5f * u, cy - halfH,
                         cx + 4.5f * u, cy + halfH);
        g.fillPath (tri);
    }
};

class SoundPanel :  public Component,
                    public DragAndDropTarget,
                    public FileDragAndDropTarget,
                    public MultiTimer,
                    public BorealAudioProcessor::SlotLoadListener
{
public:

    SoundPanel (BorealAudioProcessor* inProcessor, int soundNum)
    :   i_sound_num (soundNum),
        mProcessor (inProcessor),
        slot_index (soundNum - 1),
        soundNumberPanel (soundNum),
        soundNamePanel (this)
    {

        this->updateCurrentSound();

        addAndMakeVisible (soundNumberPanel);
        addAndMakeVisible (soundNamePanel);

        removeButton.setTooltip ("Remove this sound");
        removeButton.onClick = [this]
        {
            auto doClear = [this]
            {
                mProcessor->clearSlot (slot_index);
                isLoading = false;
                loadFailed = false;
                loadErrorMessage.clear();
                lastPlaybackDisplay = -1.0f;
                updateCurrentSound();
                repaint();
            };
            if (! mProcessor->getConfirmDestructive())
            {
                doClear();
                return;
            }
            juce::AlertWindow::showOkCancelBox (
                juce::MessageBoxIconType::QuestionIcon,
                "Remove sound?",
                "Remove the sound from slot " + juce::String (i_sound_num) + "?",
                "Remove", "Cancel", this,
                juce::ModalCallbackFunction::create ([doClear] (int result)
                {
                    if (result != 0)
                        doClear();
                }));
        };
        addAndMakeVisible (removeButton);

        const int pbase = (int) Boreal::Parameters::slot1_rate + slot_index * 5;
        const auto percentText = [] (double v)
        { return juce::String (juce::roundToInt (v)) + "%"; };
        cursorRateKnob = std::make_unique<SquareKnob> (
            inProcessor->apvts, (Boreal::Parameters) (pbase + 0), "RATE", percentText);
        cursorOffsetKnob = std::make_unique<SquareKnob> (
            inProcessor->apvts, (Boreal::Parameters) (pbase + 1), "OFFSET", percentText);
        cursorStartKnob = std::make_unique<SquareKnob> (
            inProcessor->apvts, (Boreal::Parameters) (pbase + 2), "START", percentText);
        cursorEndKnob = std::make_unique<SquareKnob> (
            inProcessor->apvts, (Boreal::Parameters) (pbase + 3), "END", percentText);
        cursorFormantKnob = std::make_unique<SquareKnob> (
            inProcessor->apvts,
            (Boreal::Parameters) ((int) Boreal::Parameters::slot1_formant + slot_index),
            "FORMANT",
            [] (double v) { return juce::String (juce::roundToInt (v)) + " st"; });
        for (auto* k : { cursorRateKnob.get(), cursorOffsetKnob.get(),
                         cursorStartKnob.get(), cursorEndKnob.get(),
                         cursorFormantKnob.get() })
        {
            k->setSquareYOffsetU (2.0f);
            addChildComponent (k);
        }

        const juce::String slotTag = "Slot " + juce::String (i_sound_num) + " ";
        cursorRateKnob->setTooltip (slotTag + "playhead speed -- 100% plays naturally, 0% freezes the cursor");
        cursorOffsetKnob->setTooltip (slotTag + "start position inside its own sound");
        cursorStartKnob->setTooltip (slotTag + "loop window start -- must sit below End, or the full sound plays");
        cursorEndKnob->setTooltip (slotTag + "loop window end -- must sit above Start, or the full sound plays");
        cursorFormantKnob->setTooltip (slotTag + "formant shift in semitones -- 0 leaves the sound untouched");

        cursorRevButton.setClickingTogglesState (true);
        cursorRevButton.setTooltip ("Reverse this slot's playhead");
        cursorRevButton.onClick = [this, pbase]
        {
            if (auto* p = mProcessor->apvts.getParameter (
                    Boreal::PARAMETERS<float>[(Boreal::Parameters) (pbase + 4)].ID))
                p->setValueNotifyingHost (cursorRevButton.getToggleState() ? 1.0f : 0.0f);
        };
        addAndMakeVisible (cursorRevButton);

        slotLoopButton.setTooltip ("Loop this slot -- off plays once through, then holds the last frame (needs the main Loop switch on)");
        slotLoopButton.onClick = [this]
        {
            if (auto* p = mProcessor->apvts.getParameter (
                    Boreal::PARAMETERS<float>[(Boreal::Parameters) ((int) Boreal::Parameters::slot1_loopmode + slot_index)].ID))
                p->setValueNotifyingHost (slotLoopButton.getToggleState() ? 1.0f : 0.0f);
        };
        addAndMakeVisible (slotLoopButton);

        cursorViewButton.setTooltip ("Show playhead controls");
        cursorViewButton.onClick = [this] { setCursorView (! showCursors); };
        addAndMakeVisible (cursorViewButton);

        setCursorView (showCursors);

        mProcessor->addSlotLoadListener (this);

        startTimer (timerIdSlow, GUI_REFRESH_TIMER_CALLBACK_SOUND);

        startTimer (timerIdFast, SOUND_PLAYHEAD_REFRESH_TIMER);
    }

    ~SoundPanel() override
    {
        mProcessor->removeSlotLoadListener (this);
    }

    void setCursorView (bool show)
    {
        showCursors = show;
        soundNumberPanel.setVisible (! show);
        soundNamePanel.setVisible (! show);
        cursorRateKnob->setVisible (show);
        cursorOffsetKnob->setVisible (show);
        cursorStartKnob->setVisible (show);
        cursorEndKnob->setVisible (show);
        cursorFormantKnob->setVisible (show);
        applyIconAvailability();
        cursorViewButton.setCursorView (show);
        cursorViewButton.setTooltip (show ? "Show sound" : "Show playhead controls");
        resized();
        repaint();
    }

    void paint (Graphics& g) override
    {
        const float innerCornerSize = GUI::Layout::InnerCardCorner * BorealZoom::uiScale;

        const Rectangle<float> outerCard = getLocalBounds().toFloat();
        const Rectangle<float> innerCard = GUI::Paint::insetCardBounds (outerCard);

        g.setColour (GUI::Color::Background);
        g.fillRoundedRectangle (innerCard, innerCornerSize);
        GUI::Paint::drawCardOutline (g, outerCard, innerCornerSize);

        if (showCursors)
        {
            return;
        }

        const float tagDim = slotInfo.loaded ? 1.0f : 0.45f;

        {
            Justification tag_just (Justification::centred);
            if (i_sound_num == 1)      tag_just = Justification::topLeft;
            else if (i_sound_num == 2) tag_just = Justification::topRight;
            else if (i_sound_num == 3) tag_just = Justification::bottomLeft;
            else                       tag_just = Justification::bottomRight;

            auto tagBounds = getLocalBounds().reduced (juce::roundToInt (24.0f * BorealZoom::uiScale),
                                                           juce::roundToInt (16.0f * BorealZoom::uiScale));

            if (i_sound_num == 1 || i_sound_num == 2)
            {
                // Lift LH/RH to mirror LL/RL padding: top-anchored caps carry
                // ascent whitespace that bottom-anchored text does not.
                tagBounds.translate (0, -juce::roundToInt (6.0f * BorealZoom::uiScale));
            }

            g.setFont (CustomLookAndFeel::makeFont (54.0f));
            g.setColour (GUI::Color::Logo.withAlpha (0.30f * tagDim));
            g.drawText (getPadPositionLabel (i_sound_num),
                        tagBounds, tag_just, false);

            g.setFont (CustomLookAndFeel::makeFont (48.0f));
            g.setColour (GUI::Color::KeyDown.withAlpha (tagDim));
            g.drawText (getPadPositionLabel (i_sound_num),
                        tagBounds, tag_just, false);
        }

        const int tag_inset = juce::roundToInt (17.0f * BorealZoom::uiScale);
        const int tag_height = juce::roundToInt (18.0f * BorealZoom::uiScale);
        const int tag_top = tag_inset;
        const int tag_bottom = getHeight() - tag_inset - tag_height;

        g.setColour (GUI::Color::Logo.withAlpha (0.50f * tagDim));
        g.setFont (CustomLookAndFeel::makeFont (18.0f));

        Rectangle<int> tag_rect (getLocalBounds().getX() + tag_inset, tag_top,
                                 getWidth() - tag_inset * 2, tag_height);

        Justification tag_justification (Justification::centred);

        if (i_sound_num == 1)
        {
            tag_rect.setY (tag_bottom);
            tag_justification = Justification::bottomRight;
        }
        else if (i_sound_num == 2)
        {
            tag_rect.setY (tag_bottom);
            tag_justification = Justification::bottomLeft;
        }
        else if (i_sound_num == 3)
        {
            tag_rect.setY (tag_top);
            tag_justification = Justification::topRight;
        }
        else
        {
            tag_rect.setY (tag_top);
            tag_justification = Justification::topLeft;
        }

        g.drawText ("[ SOUND " + String (i_sound_num) + " ]", tag_rect, tag_justification, false);
    }

    void resized() override
    {
        const int removeSize = juce::roundToInt (20.0f * BorealZoom::uiScale);
        const int removeY = getHeight() - removeSize - juce::roundToInt (14.0f * BorealZoom::uiScale);
        const int trioGap = juce::roundToInt (8.0f * BorealZoom::uiScale);
        const int trioW = removeSize * 4 + trioGap * 3;
        const int trioX = getWidth() / 2 - trioW / 2;
        slotLoopButton.setBounds (trioX, removeY, removeSize, removeSize);
        cursorRevButton.setBounds (trioX + (removeSize + trioGap), removeY, removeSize, removeSize);
        cursorViewButton.setBounds (trioX + (removeSize + trioGap) * 2, removeY,
                                    removeSize, removeSize);
        removeButton.setBounds (trioX + (removeSize + trioGap) * 3, removeY,
                                removeSize, removeSize);

        if (showCursors)
        {
            const int pad = juce::roundToInt (10.0f * BorealZoom::uiScale);
            juce::Component* knobs[5] = { cursorRateKnob.get(), cursorOffsetKnob.get(),
                                          cursorStartKnob.get(), cursorEndKnob.get(),
                                          cursorFormantKnob.get() };
            const int cellW = (getWidth() - pad * 2) / 5;
            const int knobH = juce::jmax (1, removeY - pad * 2);
            const int knobY = pad;
            for (int i = 0; i < 5; ++i)
                knobs[i]->setBounds (pad + i * cellW, knobY, cellW, knobH);
            return;
        }

        Grid grid;

        const float grid_margin = GUI::Layout::ContentInset * BorealZoom::uiScale;

        grid.columnGap = Grid::Px (grid_margin);

        using Track = Grid::TrackInfo;

        grid.templateRows = {Track (1_fr)};

        grid.templateColumns = {Track (1_fr), Track (1_fr), Track (1_fr)};

        grid.justifyContent = Grid::JustifyContent::spaceBetween;
        grid.justifyItems = Grid::JustifyItems::stretch;
        grid.alignContent = Grid::AlignContent::center;

        if (i_sound_num == 1 || i_sound_num == 3)
        {
            grid.items.addArray
            ({
                GridItem (soundNumberPanel),
                GridItem (soundNamePanel).withArea (1, 2, 1, 4),
            });
        }
        else
        {
            grid.items.addArray
            ({
                GridItem (soundNamePanel).withArea (1, 1, 1, 3),
                GridItem (soundNumberPanel),
            });
        }

        Rectangle<int> grid_bounds (juce::roundToInt (grid_margin), juce::roundToInt (grid_margin),
                                    juce::roundToInt ((float) getWidth() - (grid_margin * 2.0f)),
                                    juce::roundToInt ((float) getHeight() - (grid_margin * 2.0f)));

        grid.performLayout (grid_bounds);
    }

    bool isInterestedInDragSource (const SourceDetails&) override { return true; }

    static bool isSupportedDropFile (const File& f)
    {
        const String ext = f.getFileExtension().toLowerCase();
        return ext == ".sdif" || ext == ".wav" || ext == ".wave"
            || ext == ".aif" || ext == ".aiff";
    }

    bool isInterestedInFileDrag (const StringArray& files) override
    {
        if (files.isEmpty())
            return true;
        for (auto& f : files)
            if (isSupportedDropFile (File (f)))
                return true;
        return false;
    }

    void fileDragEnter (const StringArray&, int, int) override
    {
        isDragOver = true;
        repaint();
    }

    void fileDragExit (const StringArray&) override
    {
        isDragOver = false;
        repaint();
    }

    void filesDropped (const StringArray& files, int, int) override
    {
        isDragOver = false;
        for (auto& f : files)
        {
            const File file (f);
            if (! isSupportedDropFile (file))
                continue;
            handleDroppedFile (file);
            break;
        }
        repaint();
    }

    void handleDroppedFile (const File& f)
    {

        setCursorView (false);

        isLoading = true;
        loadFailed = false;
        loadErrorMessage.clear();
        applyIconAvailability();

        if (f.getFileExtension().equalsIgnoreCase (".sdif"))
            mProcessor->loadSlotAsync (slot_index, f);
        else
            mProcessor->analyzeAndLoadAsync (slot_index, f);

        repaint();
    }

    void itemDragEnter (const SourceDetails&) override
    {
        isDragOver = true;
        repaint();
    }

    void itemDragExit (const SourceDetails&) override
    {
        isDragOver = false;
        repaint();
    }

    void itemDropped (const SourceDetails& dragSourceDetails) override
    {
        std::string sound_file_path = dragSourceDetails.description.toString().toStdString();

        if (sound_file_path != "directory")
        {
            const File dropped (sound_file_path);
            if (isSupportedDropFile (dropped))
                handleDroppedFile (dropped);
        }

        isDragOver = false;
        repaint();
    }

    void paintOverChildren (Graphics& g) override
    {
        if (isDragOver)
        {

            g.setColour (juce::Colours::white.withAlpha (0.90f));
            g.drawRect (getLocalBounds(), 2);
        }
    }

    void slotLoadFinished (int slotIndex, bool success, const juce::String& errorMessage) override
    {
        if (slotIndex != this->slot_index)
            return;

        isLoading = false;
        loadFailed = ! success;
        loadErrorMessage = errorMessage;
        updateCurrentSound();
        repaint();
    }

private:

    static constexpr int timerIdSlow = 1;
    static constexpr int timerIdFast = 2;

    static String getPadPositionLabel (int i_sound_num)
    {
        switch (i_sound_num)
        {
            case 1: return "LH";
            case 2: return "RH";
            case 3: return "LL";
            default: return "RL";
        }
    }

    struct SoundNumberPanel : public Component
    {
        SoundNumberPanel (int num) : i_sound_num (num) {}

        void paint (Graphics&) override
        {
        }

        int i_sound_num;
    };

    struct SoundNamePanel : public Component
    {
        SoundNamePanel (SoundPanel* ownerPanel) : sound_panel (ownerPanel) {}

        void paint (Graphics& g) override
        {
            auto bounds = getLocalBounds().reduced (juce::roundToInt (4.0f * BorealZoom::uiScale), 0);
            const int line_height = juce::roundToInt (23.0f * BorealZoom::uiScale);
            const int line_gap = juce::roundToInt (2.0f * BorealZoom::uiScale);
            const Colour errorColour (GUI::Color::Logo);

            if (sound_panel->isLoading)
            {
                juce::String loadingText = "> LOADING...";
                float analyzeFraction = -1.0f;

                if (sound_panel->mProcessor->isSlotAnalyzing (sound_panel->slot_index))
                {
                    const juce::String stage = sound_panel->mProcessor->slotAnalyzeStageText().toUpperCase();
                    analyzeFraction = sound_panel->mProcessor->slotAnalyzeProgress();
                    loadingText = "> " + (stage.isEmpty() ? juce::String ("ANALYZING") : stage)
                                  + "... " + juce::String ((int) std::round (100.0f * analyzeFraction)) + "%";
                }

                g.setFont (CustomLookAndFeel::makeFont (21.0f));
                g.setColour (GUI::Color::Logo.withAlpha (0.55f));
                g.drawText (loadingText, bounds, Justification::centred, false);

                if (analyzeFraction >= 0.0f)
                {
                    const float barWidth = 90.0f * BorealZoom::uiScale;
                const float barHeight = 3.0f * BorealZoom::uiScale;
                    const Rectangle<float> bar (static_cast<float> (bounds.getCentreX()) - barWidth / 2.0f,
                                                static_cast<float> (bounds.getCentreY()) + static_cast<float> (line_height),
                                                barWidth, barHeight);
                    g.setColour (GUI::Color::Logo.withAlpha (0.20f));
                    g.fillRect (bar);
                    g.setColour (GUI::Color::KeyDown);
                    g.fillRect (bar.withWidth (juce::jmax (barHeight, barWidth * juce::jlimit (0.0f, 1.0f, analyzeFraction))));
                }
                return;
            }

            if (sound_panel->loadFailed)
            {
                const int block_y = bounds.getCentreY() - line_height - line_gap / 2;

                g.setFont (CustomLookAndFeel::makeFont (21.0f));
                g.setColour (errorColour);
                g.drawText ("> LOAD FAILED", bounds.withY (block_y).withHeight (line_height),
                            Justification::centred, false);

                if (sound_panel->loadErrorMessage.isNotEmpty())
                {
                    g.setFont (CustomLookAndFeel::makeFont (15.0f));
                    g.setColour (errorColour.withAlpha (0.7f));
                    g.drawFittedText (sound_panel->loadErrorMessage,
                                      bounds.withY (block_y + line_height + line_gap).withHeight (line_height * 2),
                                      Justification::centred, 2);
                }
                return;
            }

            const MorphEngine::SlotInfo& slot = sound_panel->slotInfo;
            if (! slot.loaded)
            {

                g.setFont (CustomLookAndFeel::makeFont (19.0f));
                g.setColour (GUI::Color::Logo.withAlpha (0.40f));
                g.drawText ("> DROP SOUND HERE <", bounds, Justification::centred, false);
                return;
            }

            const int block_y = bounds.getCentreY() - (line_height * 2 + line_gap) / 2;

            g.setFont (CustomLookAndFeel::makeFont (21.0f));
            g.setColour (GUI::Color::KeyDown);

            g.drawFittedText ("> " + slot.fileName.toUpperCase(),
                              bounds.withY (block_y).withHeight (line_height),
                              Justification::centred, 1);

            juce::String subLine = ":: " + slot.extension.toUpperCase();
            if (slot.onsetCount > 0)
            {
                subLine += " - " + juce::String (slot.onsetCount) + " HITS";
                if (slot.hasAttackLayer)
                    subLine += " +LAYER";
            }

            const int subLineY = block_y + line_height + line_gap
                               - juce::roundToInt (3.0f * BorealZoom::uiScale);
            g.setColour (GUI::Color::Logo.withAlpha (0.60f));
            g.setFont (CustomLookAndFeel::makeFont (16.0f));
            g.drawText (subLine,
                        bounds.withY (juce::roundToInt (subLineY)).withHeight (line_height),
                        Justification::centred, false);
        }

        SoundPanel* sound_panel;
    };

    void timerCallback (int timerID) override
    {
        if (timerID == timerIdFast)
        {

            const double pos = mProcessor->getTimeIndexDisplay();
            if (std::abs (pos - (double) lastPlaybackDisplay) > 0.0001)
                lastPlaybackDisplay = (float) pos;

            if (mProcessor->isSlotAnalyzing (slot_index))
            {
                const float p = mProcessor->slotAnalyzeProgress();
                if (std::abs (p - lastAnalyzeProgress) > 0.005f)
                {
                    lastAnalyzeProgress = p;
                    soundNamePanel.repaint();
                }
            }
            else
            {
                lastAnalyzeProgress = -1.0f;
            }
            return;
        }

        const bool loaded = (mProcessor->getSlotLoadMask() & (1 << slot_index)) != 0;
        if (loaded != slotInfo.loaded)
        {
            this->updateCurrentSound();
            repaint();
        }

        const int pbase = (int) Boreal::Parameters::slot1_rate + slot_index * 5;
        if (auto* p = mProcessor->apvts.getParameter (
                Boreal::PARAMETERS<float>[(Boreal::Parameters) (pbase + 4)].ID))
            cursorRevButton.setToggleState (p->getValue() >= 0.5f,
                                            NotificationType::dontSendNotification);
        if (auto* p = mProcessor->apvts.getParameter (
                Boreal::PARAMETERS<float>[(Boreal::Parameters) ((int) Boreal::Parameters::slot1_loopmode + slot_index)].ID))
            slotLoopButton.setToggleState (p->getValue() >= 0.5f,
                                           NotificationType::dontSendNotification);
    }

    void updateCurrentSound()
    {
        this->slotInfo = mProcessor->getSlotInfo (slot_index);
        this->current_sound_path = mProcessor->getSlotFilePath (slot_index);
        applyIconAvailability();
    }

    void applyIconAvailability()
    {
        const bool revOn = slotInfo.loaded && ! isLoading;
        cursorRevButton.setEnabled (revOn);
        cursorRevButton.setAlpha (revOn ? 1.0f : 0.35f);

        const bool xOn = slotInfo.loaded && ! showCursors && ! isLoading;
        removeButton.setEnabled (xOn);
        removeButton.setAlpha (xOn ? 1.0f : 0.35f);
    }

    juce::String current_sound_path;

    int i_sound_num;
    BorealAudioProcessor* mProcessor;
    int slot_index;

    MorphEngine::SlotInfo slotInfo;

    SoundNumberPanel soundNumberPanel;
    SoundNamePanel soundNamePanel;

    juce::TextButton removeButton {"X"};
    ViewFlipButton cursorViewButton;
    ReverseToggleButton cursorRevButton;
    TransportIconButton slotLoopButton { TransportIconButton::Glyph::Loop };

    std::unique_ptr<SquareKnob> cursorRateKnob;
    std::unique_ptr<SquareKnob> cursorOffsetKnob;
    std::unique_ptr<SquareKnob> cursorStartKnob;
    std::unique_ptr<SquareKnob> cursorEndKnob;
    std::unique_ptr<SquareKnob> cursorFormantKnob;
    bool showCursors = false;

    bool isDragOver = false;

    bool isLoading = false;
    bool loadFailed = false;
    juce::String loadErrorMessage;
    float lastPlaybackDisplay = -1.0f;
    float lastAnalyzeProgress = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundPanel)
};
