#pragma once

#include <JuceHeader.h>

#include "../Analysis/BorealAnalyzer.h"

#include "../Helpers/CustomLookAndFeel.h"
#include "../Helpers/InterfaceDefines.h"

class PartialsView : public juce::Component, private juce::Timer
{
public:
    PartialsView()
    {
        setInterceptsMouseClicks (false, false);
        startTimerHz (8);
    }

    ~PartialsView() override { stopTimer(); }

    void setData (std::shared_ptr<const boreal::analyzer::PartialFrameData> frames)
    {
        if (frames == data)
            return;
        data = std::move (frames);
        layerDirty = true;
        repaint();
    }

    void setFundamentalHz (double hz)
    {
        if (juce::approximatelyEqual (fundamentalHz, hz))
            return;
        fundamentalHz = hz;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (2.0f);

        g.setColour (GUI::Color::Background);
        g.fillRoundedRectangle (GUI::Paint::insetCardBounds (bounds), GUI::Layout::InnerCardCorner);
        GUI::Paint::drawCardOutline (g, bounds, GUI::Layout::InnerCardCorner);

        const bool haveData = data != nullptr && ! data->times.empty();

        if (! haveData || ! layerImage.isNull())
            if (layerImage.isValid() && layerBounds == getLocalBounds())
                g.drawImageAt (layerImage, 0, 0);

        if (! haveData)
        {
            return;
        }

        const float inkX = bounds.getX() + 4.0f;
        const float inkW = bounds.getWidth() - 8.0f;
        const float inkY = bounds.getY() + 4.0f;
        const float inkH = bounds.getHeight() - 8.0f;

        if (fundamentalHz > 0.0 && fundamentalHz >= freqLo && fundamentalHz <= freqHi)
        {
            const float fy = yForFreq ((float) fundamentalHz, inkY, inkH);
            g.setColour (GUI::Color::KeyDown.withAlpha (0.80f));
            g.drawLine (inkX, fy, inkX + inkW, fy, 1.0f);
        }

        for (double onset : data->onsets)
        {
            const float px = xForTime (onset, inkX, inkW);
            if (px < 0.0f)
                continue;
            g.setColour (GUI::Color::KeyDown.withAlpha (0.95f));
            g.drawLine (px, inkY, px, inkY + 7.0f, 1.5f);
        }

        if (data->maxActiveCount > 0 && durationSec > 0.0)
        {
            const float px = xForTime (data->maxActiveTime, inkX, inkW);
            if (px >= 0.0f)
            {
                juce::Path tri;
                tri.addTriangle (px, inkY + 9.0f, px + 5.0f, inkY + 2.0f, px - 5.0f, inkY + 2.0f);
                g.setColour (GUI::Color::KeyDown.withAlpha (0.9f));
                g.fillPath (tri);
            }
        }
    }

    void resized() override
    {
        layerDirty = true;
        lastResizeMs = juce::Time::getMillisecondCounter();
    }

private:
    void timerCallback() override
    {
        if (! layerDirty)
            return;
        if (getWidth() <= 0 || getHeight() <= 0)
            return;
        if (juce::Time::getMillisecondCounter() - lastResizeMs < 120)
            return;

        rebuildLayer();
        layerDirty = false;
        repaint();
    }

    float xForTime (double t, float inkX, float inkW) const
    {
        const double u = durationSec > 0.0 ? juce::jlimit (0.0, 1.0, t / durationSec) : 0.0;
        return inkX + (float) u * inkW;
    }

    float yForFreq (float f, float inkY, float inkH) const
    {
        const double lo = juce::jmax (20.0, (double) freqLo);
        const double hi = juce::jmax (lo * 1.001, (double) freqHi);
        const double u = std::log (juce::jmax (f, (float) lo) / lo) / std::log (hi / lo);
        return inkY + (1.0f - (float) juce::jlimit (0.0, 1.0, u)) * inkH;
    }

    void rebuildLayer()
    {
        if (! data || data->times.empty())
        {
            layerImage = juce::Image();
            return;
        }

        layerImage = juce::Image (juce::Image::ARGB, getWidth(), getHeight(), true);
        layerBounds = getLocalBounds();
        juce::Graphics g (layerImage);
        g.fillAll (juce::Colours::black);

        auto bounds = getLocalBounds().toFloat();
        const float inkX = bounds.getX() + 4.0f;
        const float inkW = bounds.getWidth() - 8.0f;
        const float inkY = bounds.getY() + 4.0f;
        const float inkH = bounds.getHeight() - 8.0f;

        const size_t nFrames = data->times.size();
        const int nPartials = (int) data->offsets.size() - 1;
        if (nPartials <= 0)
            return;

        freqLo = juce::jmax (20.0f, data->minFrequencyHz > 0.0 ? (float) data->minFrequencyHz : 20.0f);
        freqHi = juce::jmax (freqLo * 2.0f, data->maxFrequencyHz > 0.0 ? (float) data->maxFrequencyHz : 20000.0f);
        durationSec = juce::jmax (0.001, data->durationSeconds);

        float maxAmp = 0.0f;
        for (size_t i = 0; i < nFrames; ++i)
            maxAmp = juce::jmax (maxAmp, data->amps[i]);
        if (maxAmp <= 0.0f)
            maxAmp = 1.0f;
        const float minAmp = maxAmp * juce::Decibels::decibelsToGain (-90.0f);

        const float tMin = juce::jmax (1.5f, inkH / 512.0f);
        const float tMax = juce::jmax (tMin * 2.0f, inkH / 32.0f);
        auto ampToThickness = [&] (float amp)
        {
            const double p = juce::jlimit (0.0, 1.0,
                std::log ((double) juce::jmax (amp, minAmp) / (double) minAmp) / std::log ((double) maxAmp / (double) minAmp));
            return tMin * (float) std::pow ((double) tMax / (double) tMin, p);
        };

        const float strokeWidth = 1.5f;
        constexpr float kMinLineLengthPx = 2.0f;

        const auto* times = data->times.data();
        const auto* freqs = data->freqs.data();
        const auto* amps = data->amps.data();
        const auto* bws = data->bws.data();

        constexpr float kFillAlpha = 0.18f;
        constexpr float kRibAlpha = 0.18f;
        constexpr float kSpineAlpha = 0.12f;

        const bool dense = nPartials > 64;
        const float ribStepBase = dense ? kMinLineLengthPx * 2.0f : kMinLineLengthPx;

        for (int p = 0; p < nPartials; ++p)
        {
            const int i0 = data->offsets[(size_t) p];
            const int i1 = data->offsets[(size_t) p + 1];
            if (i1 <= i0)
                continue;

            juce::Path ribs;
            float lastX = -100.0f;
            for (int i = i0; i < i1; ++i)
            {
                const float x = xForTime (times[i], inkX, inkW);
                if (x < 0.0f)
                    continue;
                if (x <= lastX + ribStepBase)
                    continue;
                lastX = x;
                const float y = yForFreq (freqs[i], inkY, inkH);
                float th = ampToThickness (amps[i]) * 0.5f;
                th = juce::jmax (th, strokeWidth * 0.75f);
                ribs.addLineSegment ({ x, y - th, x, y + th }, 1.0f);
            }
            if (! ribs.isEmpty())
            {
                g.setColour (juce::Colours::white.withAlpha (kRibAlpha));
                g.strokePath (ribs, juce::PathStrokeType (strokeWidth * 0.9f));
            }
        }

        for (int p = 0; p < nPartials; ++p)
        {
            const int i0 = data->offsets[(size_t) p];
            const int i1 = data->offsets[(size_t) p + 1];
            if (i1 <= i0)
                continue;

            juce::Path tube;
            bool penDown = false;
            for (int i = i0; i < i1; ++i)
            {
                const float x = xForTime (times[i], inkX, inkW);
                if (x < 0.0f) { penDown = false; continue; }
                const float y = yForFreq (freqs[i], inkY, inkH);
                const float th = ampToThickness (amps[i]) * 0.5f;
                if (! penDown) { tube.startNewSubPath (x, y - th); penDown = true; }
                else           tube.lineTo (x, y - th);
            }
            penDown = false;
            for (int i = i1 - 1; i >= i0; --i)
            {
                const float x = xForTime (times[i], inkX, inkW);
                if (x < 0.0f) { penDown = false; continue; }
                const float y = yForFreq (freqs[i], inkY, inkH);
                const float th = ampToThickness (amps[i]) * 0.5f;
                if (! penDown) { tube.lineTo (x, y + th); penDown = true; }
                else           tube.lineTo (x, y + th);
            }
            tube.closeSubPath();
            g.setColour (juce::Colours::white.withAlpha (kFillAlpha));
            g.fillPath (tube);
        }

        for (int p = 0; p < nPartials; ++p)
        {
            const int i0 = data->offsets[(size_t) p];
            const int i1 = data->offsets[(size_t) p + 1];
            if (i1 <= i0)
                continue;

            juce::Path spine;
            bool penDown = false;
            for (int i = i0; i < i1; ++i)
            {
                const float x = xForTime (times[i], inkX, inkW);
                if (x < 0.0f) { penDown = false; continue; }
                const float y = yForFreq (freqs[i], inkY, inkH);
                if (! penDown) { spine.startNewSubPath (x, y); penDown = true; }
                else           spine.lineTo (x, y);
            }
            if (! spine.isEmpty())
            {
                g.setColour (juce::Colours::white.withAlpha (kSpineAlpha));
                g.strokePath (spine, juce::PathStrokeType (strokeWidth));
            }
        }

        if (! dense)
        {
            juce::Path xs;
            for (int p = 0; p < nPartials; ++p)
            {
                const int i0 = data->offsets[(size_t) p];
                const int i1 = data->offsets[(size_t) p + 1];
                for (int i = i0; i < i1; ++i)
                {
                    if (bws[i] <= 0.0f)
                        continue;
                    const float x = xForTime (times[i], inkX, inkW);
                    if (x < 0.0f)
                        continue;
                    const float y = yForFreq (freqs[i], inkY, inkH);
                    const float size = bws[i] * bws[i] * (inkH / 48.0f);
                    xs.addLineSegment ({ x - size, y - size, x + size, y + size }, 1.0f);
                    xs.addLineSegment ({ x - size, y + size, x + size, y - size }, 1.0f);
                }
            }
            if (! xs.isEmpty())
            {
                g.setColour (GUI::Color::KeyDown);
                g.strokePath (xs, juce::PathStrokeType (strokeWidth * 0.7f));
            }
        }
    }

    std::shared_ptr<const boreal::analyzer::PartialFrameData> data;
    double fundamentalHz = 0.0;
    uint32_t lastResizeMs = 0;

    juce::Image layerImage;
    juce::Rectangle<int> layerBounds;
    bool layerDirty = true;

    float freqLo = 20.0f;
    float freqHi = 20000.0f;
    double durationSec = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PartialsView)
};
