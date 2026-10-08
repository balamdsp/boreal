#pragma once

#include <JuceHeader.h>
#include "Helpers/CustomLookAndFeel.h"
#include "Helpers/BorealConstants.h"

inline CustomLookAndFeel& borealDialogLnf()
{
    static CustomLookAndFeel lnf;
    return lnf;
}

class BorealCardDialog : public juce::Component
{
public:
    BorealCardDialog (const juce::String& title,
                      const juce::String& message,
                      const juce::String& confirmText,
                      const juce::String& cancelText = {})
    {
        setLookAndFeel (&borealDialogLnf());

        titleLabel.setText (title, juce::dontSendNotification);
        titleLabel.setFont (CustomLookAndFeel::makeFont (19.0f));
        titleLabel.setColour (juce::Label::textColourId,
                              BorealColors::textBrand);
        addAndMakeVisible (titleLabel);

        messageLabel.setText (message, juce::dontSendNotification);
        messageLabel.setFont (CustomLookAndFeel::makeFont (20.0f));
        messageLabel.setColour (juce::Label::textColourId,
                                BorealColors::textPrimary);
        messageLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (messageLabel);

        if (cancelText.isNotEmpty())
        {
            cancelButton.setButtonText (cancelText);
            cancelButton.onClick = [this] { closeWindow(); };
            addAndMakeVisible (cancelButton);
        }

        confirmButton.setButtonText (confirmText);
        confirmButton.onClick = [this]
        {
            if (onConfirm)
                onConfirm();
            closeWindow();
        };
        addAndMakeVisible (confirmButton);
    }

    ~BorealCardDialog() override
    {
        setLookAndFeel (nullptr);
    }

    std::function<void()> onConfirm;

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour (BorealColors::cardDark);
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (BorealColors::accent.withAlpha (0.40f));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

        auto innerBounds = bounds.reduced (12.0f);
        g.setColour (BorealColors::background);
        g.fillRoundedRectangle (innerBounds, 4.0f);
        g.setColour (BorealColors::accent.withAlpha (0.10f));
        g.drawRoundedRectangle (innerBounds, 4.0f, 1.0f);
    }

    void resized() override
    {
        const float zs = BorealZoom::uiScale;
        auto area = getLocalBounds().reduced (juce::roundToInt (25.0f * zs));

        titleLabel.setBounds (area.removeFromTop (juce::roundToInt (21.0f * zs)));
        area.removeFromTop (juce::roundToInt (5.0f * zs));

        const int rowGap = juce::roundToInt (10.0f * zs);
        auto buttonsRow = area.removeFromBottom (juce::roundToInt (30.0f * zs));
        area.removeFromBottom (rowGap);

        const int cancelW = cancelButton.isVisible() ? juce::roundToInt (150.0f * zs) : 0;
        if (cancelButton.isVisible())
        {
            cancelButton.setBounds (buttonsRow.removeFromRight (cancelW));
            buttonsRow.removeFromRight (rowGap);
        }
        confirmButton.setBounds (buttonsRow.removeFromRight (juce::roundToInt (190.0f * zs)));

        messageLabel.setBounds (area);
    }

private:
    void closeWindow()
    {
        if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
            dw->closeButtonPressed();
    }

    juce::Label titleLabel;
    juce::Label messageLabel;
    juce::TextButton confirmButton;
    juce::TextButton cancelButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BorealCardDialog)
};

class BorealDialogWindow : public juce::DocumentWindow
{
public:
    BorealDialogWindow (const juce::String& title, juce::Component* content)
        : juce::DocumentWindow (title, BorealColors::menuBg, allButtons)
    {
        setContentOwned (content, true);
        setResizable (false, false);
    }

    void closeButtonPressed() override { delete this; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BorealDialogWindow)
};

namespace BorealDialogs
{
    inline void openWindow (juce::Component* ownedContent,
                            const juce::String& title,
                            juce::Component* centreOn,
                            int w, int h)
    {
        auto* win = new BorealDialogWindow (title, ownedContent);

        if (centreOn != nullptr && centreOn->getPeer() != nullptr)
        {
            const auto centre = centreOn->getScreenBounds().getCentre();
            win->setTopLeftPosition (centre.getX() - w / 2, centre.getY() - h / 2);
            win->setSize (w, h);
        }
        else
        {
            win->centreWithSize (w, h);
        }

        win->setVisible (true);
    }
}

class BorealSaveAsDialog : public juce::Component
{
public:
    BorealSaveAsDialog (const juce::String& currentName,
                        std::function<void (const juce::String&)> onSave)
    :   onSavePreset (std::move (onSave))
    {
        setLookAndFeel (&borealDialogLnf());

        titleLabel.setText (">> SAVE AS", juce::dontSendNotification);
        titleLabel.setFont (CustomLookAndFeel::makeFont (19.0f));
        titleLabel.setColour (juce::Label::textColourId,
                              BorealColors::textBrand);
        addAndMakeVisible (titleLabel);

        promptLabel.setText ("Enter a name for your preset:", juce::dontSendNotification);
        promptLabel.setFont (CustomLookAndFeel::makeFont (22.0f));
        promptLabel.setColour (juce::Label::textColourId,
                               BorealColors::textPrimary);
        promptLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (promptLabel);

        nameEditor.setFont (CustomLookAndFeel::makeFont (22.0f));
        nameEditor.setText (currentName);
        nameEditor.setSelectAllWhenFocused (true);
        nameEditor.setColour (juce::TextEditor::textColourId, BorealColors::menuTextBright);
        nameEditor.setColour (juce::TextEditor::backgroundColourId, BorealColors::menuBg);
        nameEditor.setColour (juce::TextEditor::outlineColourId, BorealColors::menuBorder);
        nameEditor.onReturnKey = [this] { confirm(); };
        addAndMakeVisible (nameEditor);

        cancelButton.setButtonText ("CANCEL");
        cancelButton.onClick = [this] { closeWindow(); };
        addAndMakeVisible (cancelButton);

        confirmButton.setButtonText ("CONFIRM");
        confirmButton.onClick = [this] { confirm(); };
        addAndMakeVisible (confirmButton);
    }

    ~BorealSaveAsDialog() override
    {
        setLookAndFeel (nullptr);
    }

    void visibilityChanged() override
    {
        if (isShowing())
            nameEditor.grabKeyboardFocus();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour (BorealColors::cardDark);
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (BorealColors::accent.withAlpha (0.40f));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

        auto innerBounds = bounds.reduced (12.0f);
        g.setColour (BorealColors::background);
        g.fillRoundedRectangle (innerBounds, 4.0f);
        g.setColour (BorealColors::accent.withAlpha (0.10f));
        g.drawRoundedRectangle (innerBounds, 4.0f, 1.0f);
    }

    void resized() override
    {
        const float zs = BorealZoom::uiScale;
        auto area = getLocalBounds().reduced (juce::roundToInt (25.0f * zs));

        titleLabel.setBounds (area.removeFromTop (juce::roundToInt (21.0f * zs)));
        area.removeFromTop (juce::roundToInt (5.0f * zs));
        promptLabel.setBounds (area.removeFromTop (juce::roundToInt (26.0f * zs)));
        area.removeFromTop (juce::roundToInt (8.0f * zs));

        nameEditor.setBounds (area.removeFromTop (juce::roundToInt (36.0f * zs)));
        area.removeFromTop (juce::roundToInt (12.0f * zs));

        auto buttonsRow = area.removeFromBottom (juce::roundToInt (30.0f * zs));

        cancelButton.setBounds (buttonsRow.removeFromRight (juce::roundToInt (150.0f * zs)));
        buttonsRow.removeFromRight (juce::roundToInt (10.0f * zs));
        confirmButton.setBounds (buttonsRow.removeFromRight (juce::roundToInt (190.0f * zs)));
    }

private:
    void confirm()
    {
        if (onSavePreset)
            onSavePreset (nameEditor.getText());
        closeWindow();
    }

    void closeWindow()
    {
        if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
            dw->closeButtonPressed();
    }

    std::function<void (const juce::String&)> onSavePreset;

    juce::Label titleLabel;
    juce::Label promptLabel;
    juce::TextEditor nameEditor;
    juce::TextButton confirmButton;
    juce::TextButton cancelButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BorealSaveAsDialog)
};

class AboutWindow : public juce::DocumentWindow
{
public:
    AboutWindow()
        : juce::DocumentWindow ("About Boreal",
                                BorealColors::menuBg,
                                allButtons)
    {
        const juce::String message =
            "Version " + juce::String (ProjectInfo::versionString)
            + "\n\nA spectral morphing synthesis instrument"
            + "\nBy BalamDSP (AGPLv3)"
            + "\n\nBased on:"
            + "\n  JUCE framework  -- JUCE Ltd (AGPLv3)"
            + "\n  Loris library  -- Fitz & Haken (GPL-2.0-or-later)"
            + "\n  Vutu  -- Madrona Labs (GPLv3)"
            + "\n  CLAP wrapper  -- free-audio (MIT)"
            + "\n  cool-retro-term  -- Swordfish90 (GPL)"
            + "\n  VT323 typeface  -- Peter Hull (OFL)";

        setContentOwned (
            new BorealCardDialog (">> ABOUT BOREAL", message, "CLOSE"),
            true);
        setResizable (false, false);

        centreWithSize (juce::roundToInt (460.0f * BorealZoom::uiScale),
                        juce::roundToInt (560.0f * BorealZoom::uiScale));
        setVisible (true);
    }

    void closeButtonPressed() override { delete this; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutWindow)
};
