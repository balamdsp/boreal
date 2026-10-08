#pragma once

#include <JuceHeader.h>

#include "../Helpers/CustomLookAndFeel.h"
#include "../Helpers/InterfaceDefines.h"
#include "../Helpers/BorealConstants.h"

class WaveformView : public juce::Component
{
public:
    WaveformView (const juce::String& titleText)
    :   title (titleText)
    {
        setInterceptsMouseClicks (true, false);
    }

    void setWaveform (std::shared_ptr<const std::vector<float>> samples, double sampleRate)
    {
        waveform = std::move (samples);
        rate = sampleRate;
        buildMipPyramid();
        rebuildPath();
    }

    void setPlayheadSupplier (std::function<double()> supplier) { playheadSupplier = std::move (supplier); }

    void setIntervalInternal (double start, double end)
    {
        dragging = DragNone;
        intervalStart = juce::jlimit (0.0, 1.0, start);
        intervalEnd = juce::jlimit (0.0, 1.0, end);
    }

    double getIntervalStart() const { return intervalStart; }
    double getIntervalEnd() const { return intervalEnd; }

    std::function<void (double start, double end)> onIntervalChanged;
    std::function<void()> onEmptyClick;

    void setEmptyText (const juce::String& text) { emptyText = text; }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (2.0f);

        g.setColour (GUI::Color::Background);
        g.fillRoundedRectangle (GUI::Paint::insetCardBounds (bounds), GUI::Layout::InnerCardCorner);
        GUI::Paint::drawCardOutline (g, bounds, GUI::Layout::InnerCardCorner);

        if (title.isNotEmpty())
        {
            g.setFont (CustomLookAndFeel::makeFont (13.0f));
            g.setColour (GUI::Color::Logo.withAlpha (0.60f));
            g.drawText (title, bounds.reduced (8.0f, 3.0f), juce::Justification::topLeft, false);
        }

        const float inkX = bounds.getX() + 4.0f;
        const float inkW = bounds.getWidth() - 8.0f;
        const float inkY = bounds.getY() + 4.0f;
        const float inkH = bounds.getHeight() - 8.0f;
        if (inkW <= 1.0f || inkH <= 2.0f)
            return;

        if (cachedWave.isValid() && cachedBounds == getLocalBounds())
            g.drawImageAt (cachedWave, 0, 0);

        if (! waveform || waveform->empty())
        {
            g.setFont (CustomLookAndFeel::makeFont (16.0f));
            g.setColour (GUI::Color::Logo.withAlpha (0.45f));
            g.drawText (emptyText, bounds.toNearestInt(), juce::Justification::centred, false);
            return;
        }

        const float x1 = inkX + static_cast<float> (intervalStart) * inkW;
        const float x2 = inkX + static_cast<float> (intervalEnd) * inkW;

        g.setColour (GUI::Color::Background.withAlpha (0.72f));
        if (intervalStart > 0.0)
            g.fillRect (inkX, inkY, x1 - inkX, inkH);
        if (intervalEnd < 1.0)
            g.fillRect (x2, inkY, inkX + inkW - x2, inkH);

        g.setColour (GUI::Color::KeyDown.withAlpha (0.95f));
        const float foot = 6.0f;
        for (int k = 0; k < 2; ++k)
        {
            const float bx = (k == 0) ? x1 : x2;
            g.drawLine (bx, inkY, bx, inkY + inkH, 1.5f);
            const float dir = (k == 0) ? 1.0f : -1.0f;
            g.drawLine (bx, inkY, bx + dir * foot, inkY, 1.5f);
            g.drawLine (bx, inkY + inkH, bx + dir * foot, inkY + inkH, 1.5f);
        }

        if (playheadSupplier)
        {
            const double t = playheadSupplier();
            const double dur = durationSeconds();
            if (t >= 0.0 && dur > 0.0)
            {
                const float px = inkX + (float) juce::jlimit (0.0, 1.0, t / dur) * inkW;
                g.setColour (GUI::Color::Accent);
                g.drawLine (px, inkY, px, inkY + inkH, 1.0f);
            }
        }
    }

    void resized() override { rebuildPath(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = DragNone;
        if (! waveform || waveform->empty())
        {
            if (onEmptyClick)
                onEmptyClick();
            return;
        }
        const float inkX = 6.0f;
        const float inkW = (float) getWidth() - 12.0f;
        const float x = (float) e.getPosition().x;
        const float x1 = inkX + (float) intervalStart * inkW;
        const float x2 = inkX + (float) intervalEnd * inkW;
        const float grab = 8.0f * BorealZoom::uiScale;

        if (std::abs (x - x1) <= grab && std::abs (x - x1) <= std::abs (x - x2))
            dragging = DragLeft;
        else if (std::abs (x - x2) <= grab)
            dragging = DragRight;

        if (dragging != DragNone)
        {
            dragStartU = juce::jlimit (0.0, 1.0, (double) (x - inkX) / (double) inkW);
            dragStartValue = (dragging == DragLeft) ? intervalStart : intervalEnd;
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging == DragNone || ! waveform || dragStartU < 0.0)
            return;

        const float inkX = 6.0f;
        const float inkW = juce::jmax (1.0f, (float) getWidth() - 12.0f);
        const double rawU = juce::jlimit (0.0, 1.0, (double) ((float) e.getPosition().x - inkX) / (double) inkW);

        double target = rawU;
        if (e.mods.isShiftDown())
            target = dragStartValue + (rawU - dragStartU) * 0.1;

        constexpr double minSpan = 0.005;
        if (dragging == DragLeft)
            intervalStart = juce::jlimit (0.0, intervalEnd - minSpan, target);
        else
            intervalEnd = juce::jlimit (intervalStart + minSpan, 1.0, target);

        repaint();
        if (onIntervalChanged)
            onIntervalChanged (intervalStart, intervalEnd);
    }

    void mouseUp (const juce::MouseEvent&) override { dragging = DragNone; }

private:
    enum DragSide { DragNone, DragLeft, DragRight };

    double durationSeconds() const
    {
        return (rate > 0.0 && waveform) ? (double) waveform->size() / rate : 0.0;
    }

    void buildMipPyramid()
    {
        peakMips.clear();
        if (! waveform || waveform->empty())
            return;

        const size_t n = waveform->size();
        const size_t nBase = (n + (size_t) mipBase - 1) / (size_t) mipBase;

        peakMips.reserve (16);
        peakMips.emplace_back (nBase, 0.0f);
        for (size_t b = 0; b < nBase; ++b)
        {
            const size_t i0 = b * (size_t) mipBase;
            const size_t i1 = juce::jmin (n, i0 + (size_t) mipBase);
            float peak = 0.0f;
            for (size_t i = i0; i < i1; ++i)
                peak = juce::jmax (peak, std::abs ((*waveform)[i]));
            peakMips.back()[b] = peak;
        }

        while (peakMips.back().size() > 1)
        {
            const size_t li = peakMips.size() - 1;
            const size_t nNext = (peakMips[li].size() + 1) / 2;
            peakMips.emplace_back (nNext, 0.0f);
            const auto& prev = peakMips[li];
            auto& next = peakMips.back();
            for (size_t b = 0; b < nNext; ++b)
                next[b] = juce::jmax (prev[2 * b], 2 * b + 1 < prev.size() ? prev[2 * b + 1] : 0.0f);
        }
    }

    float peakForRange (size_t i0, size_t i1) const
    {
        const size_t span = i1 > i0 ? i1 - i0 : 0;
        if (span <= (size_t) mipBase || peakMips.empty() || waveform == nullptr)
        {
            float peak = 0.0f;
            const size_t n = waveform != nullptr ? waveform->size() : 0;
            for (size_t i = i0; i < i1 && i < n; ++i)
                peak = juce::jmax (peak, std::abs ((*waveform)[i]));
            return peak;
        }

        size_t level = 0;
        size_t cover = (size_t) mipBase;
        while (level + 1 < peakMips.size() && cover * 2 <= span)
        {
            ++level;
            cover *= 2;
        }

        const auto& mip = peakMips[level];
        const size_t b0 = juce::jmin (mip.size(), i0 / cover);
        const size_t b1 = juce::jmin (mip.size(), (i1 + cover - 1) / cover);
        float peak = 0.0f;
        for (size_t b = b0; b < b1; ++b)
            peak = juce::jmax (peak, mip[b]);
        return peak;
    }

    void rebuildPath()
    {
        cachedWave = juce::Image();
        if (! waveform || waveform->empty() || getWidth() <= 0 || getHeight() <= 0)
            return;

        cachedWave = juce::Image (juce::Image::ARGB, getWidth(), getHeight(), true);
        juce::Graphics g (cachedWave);

        const auto bounds = getLocalBounds().toFloat().reduced (2.0f);
        const float inkX = bounds.getX() + 4.0f;
        const float inkW = bounds.getWidth() - 8.0f;
        const float centerY = bounds.getCentreY();
        const float halfH = bounds.getHeight() * 0.5f - 3.0f;

        const int w = (int) inkW;
        const size_t n = waveform->size();

        juce::Path strokes;
        for (int x = 0; x < w; ++x)
        {
            const size_t i0 = (size_t) ((double) x / (double) w * (double) n);
            const size_t i1 = juce::jlimit (i0 + 1, n, (size_t) ((double) (x + 1) / (double) w * (double) n));

            const float px = inkX + (float) x;
            const float th = juce::jmax (1.0f, peakForRange (i0, i1) * halfH);
            strokes.addLineSegment ({ px, centerY - th, px, centerY + th }, 1.0f);
        }

        g.setColour (GUI::Color::KeyDown);
        g.strokePath (strokes, juce::PathStrokeType (1.0f));
        cachedBounds = getLocalBounds();
    }

    static constexpr int mipBase = 256;

    std::vector<std::vector<float>> peakMips;

    juce::String title;
    juce::String emptyText { "> CLICK HERE OR PRESS OPEN <" };
    std::shared_ptr<const std::vector<float>> waveform;
    double rate = 0.0;

    juce::Image cachedWave;
    juce::Rectangle<int> cachedBounds;

    double intervalStart = 0.0;
    double intervalEnd = 1.0;
    DragSide dragging = DragNone;
    double dragStartU = -1.0;
    double dragStartValue = 0.0;

    std::function<double()> playheadSupplier;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformView)
};
