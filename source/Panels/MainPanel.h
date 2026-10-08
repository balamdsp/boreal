#pragma once

#include <JuceHeader.h>

#include "MorphingPanel.h"

class MainPanel : public Component
{
public:

    MainPanel (BorealAudioProcessor* inProcessor) : morphingPanel (inProcessor)
    {
        addAndMakeVisible (morphingPanel);
    }

    ~MainPanel() {}

    void paint (Graphics&) override {}

    void resized() override
    {
        FlexBox fb;

        FlexItem morphing (static_cast<float> (getWidth()), static_cast<float> (getHeight()), morphingPanel);

        fb.items.addArray ({morphing});
        fb.performLayout (getLocalBounds().toFloat());
    }

private:

    MorphingPanel morphingPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainPanel)
};
