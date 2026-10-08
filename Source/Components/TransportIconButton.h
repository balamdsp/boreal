#pragma once

#include <JuceHeader.h>

#include <cmath>

#include "../Helpers/CustomLookAndFeel.h"
#include "../Helpers/InterfaceDefines.h"
#include "../Helpers/BorealConstants.h"

class TransportIconButton : public juce::TextButton
{
public:
    enum class Glyph { Loop, Forward, Scrub };

    explicit TransportIconButton (Glyph g) : glyph (g)
    {
        setClickingTogglesState (true);
    }

    void paint (juce::Graphics& g) override
    {
        const float zs = BorealZoom::uiScale;

        getLookAndFeel().drawButtonBackground (g, *this, findColour (buttonColourId),
                                               isMouseOver(), isDown());

        const bool on = getToggleState();
        const bool hot = isMouseOver() || isDown();
        if (! isEnabled())
            g.setColour (findColour (TextButton::textColourOffId).withAlpha (0.25f));
        else if (on)
            g.setColour (findColour (TextButton::textColourOnId));
        else if (hot)
            g.setColour (GUI::Color::Logo.withAlpha (0.95f));
        else
            g.setColour (findColour (TextButton::textColourOffId));

        auto b = getLocalBounds().toFloat().reduced (4.0f * zs);
        const float cx = b.getCentreX();
        const float cy = b.getCentreY();
        const float s = juce::jmin (b.getWidth(), b.getHeight()) / 30.0f * zs;

        switch (glyph)
        {
            case Glyph::Forward:
            {
                juce::Path tri;
                tri.addTriangle (cx - 4.5f * s, cy - 7.0f * s,
                                 cx - 4.5f * s, cy + 7.0f * s,
                                 cx + 7.5f * s, cy);
                g.fillPath (tri);
                break;
            }
            case Glyph::Loop:
            {
                const float r = 7.5f * s;
                const float a1 = juce::MathConstants<float>::halfPi;
                juce::Path ring;
                ring.addCentredArc (cx, cy, r, r, 0.0f,
                                    a1 - juce::MathConstants<float>::twoPi * 0.83f, a1,
                                    true);
                g.strokePath (ring, juce::PathStrokeType ((on ? 2.6f : 2.0f) * s));

                juce::Path head;
                const float hx = cx + r;
                const float hy = cy;
                head.addTriangle (hx - 4.0f * s, hy - 2.0f * s,
                                  hx + 4.0f * s, hy - 2.0f * s,
                                  hx, hy + 4.0f * s);
                g.fillPath (head);
                break;
            }
            case Glyph::Scrub:
            {
                juce::Path left;
                left.addTriangle (cx - 1.5f * s, cy - 7.0f * s,
                                  cx - 1.5f * s, cy + 7.0f * s,
                                  cx - 8.0f * s, cy);
                g.fillPath (left);

                juce::Path right;
                right.addTriangle (cx + 1.5f * s, cy - 7.0f * s,
                                   cx + 1.5f * s, cy + 7.0f * s,
                                   cx + 8.0f * s, cy);
                g.fillPath (right);
                break;
            }
        }
    }

private:
    Glyph glyph;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportIconButton)
};
