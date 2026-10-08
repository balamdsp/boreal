#pragma once

#include <JuceHeader.h>

#include "../Helpers/CustomLookAndFeel.h"
#include "../Helpers/InterfaceDefines.h"
#include "../Helpers/BorealConstants.h"

class CollectionBrowser : public FileBrowserComponent
{
public:

    CollectionBrowser (int browserFlags = FileBrowserComponent::openMode |
                                   FileBrowserComponent::canSelectFiles |
                                   FileBrowserComponent::useTreeView |
                                   FileBrowserComponent::filenameBoxIsReadOnly,
                       const File& initialFileOrDirectory = File (getDefaultCollectionsDirectory()),
                       const juce::FileFilter* filter = new WildcardFileFilter ("*.sdif;*.wav;*.wave;*.aif;*.aiff", "*", "sound files"),
                       FilePreviewComponent* preview = NULL)
    :   FileBrowserComponent (browserFlags, initialFileOrDirectory, filter, preview)
    {
        pTreeComponent = static_cast<FileTreeComponent*> (this->getDisplayComponent());
        if (pTreeComponent) pTreeComponent->setDragAndDropDescription ("DragAndDrop");
        if (auto* tree = dynamic_cast<juce::TreeView*> (pTreeComponent))
            tree->setColour (juce::TreeView::backgroundColourId, Colours::transparentBlack);

        setFilenameBoxLabel ("> FILE:");

        setColour (FileBrowserComponent::filenameBoxBackgroundColourId, Colours::transparentBlack);
        setColour (FileBrowserComponent::filenameBoxTextColourId, BorealColors::textPrimary);

        this->lookAndFeelChanged();
    }

    ~CollectionBrowser() override {}

    void lookAndFeelChanged() override
    {
        FileBrowserComponent::lookAndFeelChanged();

        if (auto* filename_editor = findFilenameBox())
        {
            filename_editor->setFont (CustomLookAndFeel::makeFont (21.0f));
            filename_editor->applyFontToAllText (CustomLookAndFeel::makeFont (21.0f));
            filename_editor->setJustification (Justification::centredLeft);
            filename_editor->setColour (TextEditor::textColourId, BorealColors::textPrimary);
            filename_editor->setColour (TextEditor::backgroundColourId, Colours::transparentBlack);
            filename_editor->setColour (TextEditor::outlineColourId, Colours::transparentBlack);
        }
    }

    void selectionChanged() override
    {
        FileBrowserComponent::selectionChanged();

        if (auto* filename_editor = findFilenameBox())
        {
            if (filename_editor->isReadOnly())
                filename_editor->setText (filename_editor->getText().toUpperCase(),
                                          NotificationType::dontSendNotification);
            filename_editor->setFont (CustomLookAndFeel::makeFont (21.0f));
            filename_editor->applyFontToAllText (CustomLookAndFeel::makeFont (21.0f));
            filename_editor->setJustification (Justification::centredLeft);
        }
    }

    void paint (Graphics& g) override
    {
        const float innerCornerSize = GUI::Layout::InnerCardCorner * BorealZoom::uiScale;
        const float inset = GUI::Layout::CardInset * BorealZoom::uiScale;

        const Rectangle<float> outerCard = getLocalBounds().toFloat().reduced (inset);
        const Rectangle<float> innerCard = GUI::Paint::insetCardBounds (outerCard);

        g.setColour (GUI::Color::Background);
        g.fillRoundedRectangle (innerCard, innerCornerSize);
        GUI::Paint::drawCardOutline (g, outerCard, innerCornerSize);

        if (auto* filename_editor = findFilenameBox())
        {
            if (filename_editor->isVisible() && ! filename_editor->getBounds().isEmpty())
            {
                const float zs = BorealZoom::uiScale;
                const Rectangle<float> box = filename_editor->getBounds().toFloat();

                g.setColour (GUI::Color::Body);
                g.fillRoundedRectangle (box, 2.0f * zs);

                g.setColour (GUI::Color::AccentDim);
                g.drawRoundedRectangle (box, 2.0f * zs, 1.2f * zs);
            }
        }
    }

private:

    static bool isInsideComboBox (Component* c)
    {
        for (auto* p = c->getParentComponent(); p != nullptr; p = p->getParentComponent())
            if (dynamic_cast<juce::ComboBox*> (p) != nullptr)
                return true;
        return false;
    }

    static TextEditor* findFilenameBox (Component& comp)
    {
        for (int i = 0; i < comp.getNumChildComponents(); ++i)
        {
            if (auto* child = comp.getChildComponent (i))
            {
                if (auto* editor = dynamic_cast<TextEditor*> (child))
                {
                    if (! isInsideComboBox (editor))
                        return editor;
                }

                if (auto* editor = findFilenameBox (*child))
                    return editor;
            }
        }

        return nullptr;
    }

    TextEditor* findFilenameBox() const { return findFilenameBox (const_cast<CollectionBrowser&> (*this)); }

    void fileClicked (const File& f, const MouseEvent& e) override
    {
        FileBrowserComponent::fileClicked (f, e);
        if (f.isDirectory())
        {
            pTreeComponent->setDragAndDropDescription ("directory");
        }
        else
        {
            pTreeComponent->setDragAndDropDescription (pTreeComponent->getSelectedFile().getFullPathName());
        }
    }

    FileTreeComponent* pTreeComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CollectionBrowser)
};
