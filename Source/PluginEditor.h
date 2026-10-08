#pragma once

#include <cstdint>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Standalone/CustomStandaloneFilterWindow.h"
#include "PluginProcessor.h"
#include "Panels/BorealPanel.h"

extern "C" int borealGetFrameExtents (std::uintptr_t windowH,
                                      int* outFrameW, int* outFrameH);

static juce::Point<int> getNativeFrameSize (juce::Component* topLevelWindow)
{
    if (topLevelWindow != nullptr)
        if (auto* peer = topLevelWindow->getPeer())
            if (peer->getNativeHandle() != nullptr)
            {
                int frameW = 0, frameH = 0;

                if (borealGetFrameExtents (reinterpret_cast<std::uintptr_t> (peer->getNativeHandle()),
                                           &frameW, &frameH) != 0)
                    return { frameW, frameH };
            }

    return {};
}

static bool isEditorInStandaloneApp (const juce::Component* c)
{
    return c != nullptr
        && dynamic_cast<const juce::BorealFilterWindow*> (c->getTopLevelComponent()) != nullptr;
}

class BorealAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         public juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit BorealAudioProcessorEditor (BorealAudioProcessor& p)
        : AudioProcessorEditor (&p),
          processor (p),
          panel (new BorealPanel (&p))
    {
        addAndMakeVisible (panel.get());

        setResizable (false, false);

        uiScale = readZoomScale();
        updateZoomLimits();
        applyZoom (uiScale);

        processor.apvts.addParameterListener (Boreal::Zoom::UI_SCALE_ID, this);
    }

    ~BorealAudioProcessorEditor() override
    {
        processor.apvts.removeParameterListener (Boreal::Zoom::UI_SCALE_ID, this);
    }

    void parameterChanged (const juce::String& parameterID, float) override
    {
        if (parameterID != Boreal::Zoom::UI_SCALE_ID)
            return;
        applyZoom (readZoomScale());
    }

    void applyZoom (float scale)
    {
        scale = juce::jlimit (Boreal::Zoom::ZOOM_MIN, Boreal::Zoom::ZOOM_MAX, scale);
        uiScale = scale;
        BorealZoom::uiScale = scale;
        updateZoomLimits();

        const int pixW = juce::roundToInt ((float) BOREAL_PANEL_WIDTH * uiScale);
        const int pixH = juce::roundToInt ((float) BOREAL_PANEL_HEIGHT * uiScale);

        const auto frame = getNativeFrameSize (isEditorInStandaloneApp (this)
                                               ? getTopLevelComponent() : nullptr);
        const int outerW = juce::jmax (1, pixW + frame.x);
        const int outerH = juce::jmax (1, pixH + frame.y);

        if (isEditorInStandaloneApp (this))
            if (auto* tl = getTopLevelComponent())
                if (tl != this)
                    if (auto* rw = dynamic_cast<juce::ResizableWindow*> (tl))
                        if (auto* c = rw->getConstrainer())
                            c->setSizeLimits (outerW, outerH, outerW, outerH);

        setSize (pixW, pixH);

        if (isEditorInStandaloneApp (this))
            if (auto* tl = getTopLevelComponent())
                if (tl != this)
                    tl->setSize (outerW, outerH);

        resized();
        repaint();
    }

    void updateZoomLimits()
    {
        if (isEditorInStandaloneApp (this))
            setResizeLimits (juce::roundToInt ((float) BOREAL_PANEL_WIDTH * uiScale),
                             juce::roundToInt ((float) BOREAL_PANEL_HEIGHT * uiScale),
                             juce::roundToInt ((float) BOREAL_PANEL_WIDTH * uiScale),
                             juce::roundToInt ((float) BOREAL_PANEL_HEIGHT * uiScale));
        else
            setResizeLimits (juce::roundToInt ((float) BOREAL_PANEL_WIDTH * Boreal::Zoom::ZOOM_MIN),
                             juce::roundToInt ((float) BOREAL_PANEL_HEIGHT * Boreal::Zoom::ZOOM_MIN),
                             juce::roundToInt ((float) BOREAL_PANEL_WIDTH * Boreal::Zoom::ZOOM_MAX),
                             juce::roundToInt ((float) BOREAL_PANEL_HEIGHT * Boreal::Zoom::ZOOM_MAX));

        setResizable (false, false);
    }

    int uiScaleIndex() const
    {
        int best = 0;
        float bestDist = 1.0e9f;
        for (size_t i = 0; i < Boreal::Zoom::ZOOM_PERCENTS.size(); ++i)
        {
            const float d = std::abs (Boreal::Zoom::ZOOM_PERCENTS[i] / 100.0f - uiScale);
            if (d < bestDist)
            {
                bestDist = d;
                best = (int) i;
            }
        }
        return best;
    }

    void parentHierarchyChanged() override
    {
        if (topLevelIsWindow)
            return;

        if (auto* sfw = dynamic_cast<juce::ResizableWindow*> (getTopLevelComponent()))
        {
            topLevelIsWindow = true;

            juce::Component::SafePointer<juce::ResizableWindow> safeSfw { sfw };
            juce::Component::SafePointer<BorealAudioProcessorEditor> safeThis { this };
            juce::MessageManager::callAsync ([safeSfw, safeThis]()
            {
                if (safeSfw == nullptr || safeThis == nullptr)
                    return;

                const int w = juce::roundToInt ((float) BOREAL_PANEL_WIDTH * safeThis->uiScale);
                const int h = juce::roundToInt ((float) BOREAL_PANEL_HEIGHT * safeThis->uiScale);
                if (isEditorInStandaloneApp (safeThis.getComponent()))
                {
                    if (! safeSfw->isResizable())
                        safeSfw->setResizable (true, false);
                    const auto frame = getNativeFrameSize (safeSfw.getComponent());
                    const int outerW = juce::jmax (1, w + frame.x);
                    const int outerH = juce::jmax (1, h + frame.y);
                    if (auto* c = safeSfw->getConstrainer())
                        c->setSizeLimits (outerW, outerH, outerW, outerH);
                    safeThis->setSize (w, h);
                    safeSfw->setSize (outerW, outerH);
                }
                else
                {
                    safeSfw->setResizable (false, false);
                    safeSfw->setSize (w, h);
                }
            });

            if (isEditorInStandaloneApp (this))
                juce::Timer::callAfterDelay (250, [safeThis = juce::Component::SafePointer<BorealAudioProcessorEditor> (this)]()
                {
                    if (safeThis == nullptr)
                        return;

                    safeThis->applyZoom (safeThis->uiScale);
                });

           #if JUCE_WINDOWS
            if (auto* peer = getPeer())
                peer->setCustomPlatformScaleFactor (1.0f);
           #endif
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black);
    }

    void resized() override
    {
        panel->setBounds (getLocalBounds());
    }

private:
    float readZoomScale() const
    {
        if (auto* param = processor.apvts.getParameter (Boreal::Zoom::UI_SCALE_ID))
        {
            const int idx = juce::jlimit (0, (int) Boreal::Zoom::ZOOM_PERCENTS.size() - 1,
                                          juce::roundToInt (param->getValue()
                                              * (float) (Boreal::Zoom::ZOOM_PERCENTS.size() - 1)));
            return Boreal::Zoom::ZOOM_PERCENTS[(size_t) idx] / 100.0f;
        }
        return 1.0f;
    }

    BorealAudioProcessor& processor;
    std::unique_ptr<BorealPanel> panel;
    float uiScale = 1.0f;
    bool topLevelIsWindow = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BorealAudioProcessorEditor)
};
