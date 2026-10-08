#pragma once

#include "JuceHeader.h"
#include "BorealConstants.h"

#define BOREAL_PANEL_WIDTH 1000
#define BOREAL_PANEL_HEIGHT 700
#define BOREAL_RATIO double(BOREAL_PANEL_WIDTH) / double(BOREAL_PANEL_HEIGHT)

#define TOP_PANEL_WIDTH BOREAL_PANEL_WIDTH
#define TOP_PANEL_HEIGHT 50

#define BOTTOM_PANEL_WIDTH BOREAL_PANEL_WIDTH
#define BOTTOM_PANEL_HEIGHT 50

#define SOUND_PANEL_WIDTH 250
#define SOUND_PANEL_HEIGHT BOREAL_PANEL_HEIGHT - TOP_PANEL_HEIGHT - BOTTOM_PANEL_HEIGHT

#define CENTER_PANEL_WIDTH BOREAL_PANEL_WIDTH - (2 * SOUND_PANEL_WIDTH)
#define CENTER_PANEL_HEIGHT SOUND_PANEL_HEIGHT

namespace GUI
{
    namespace Color
    {

        const Colour Transparent = Colour(0, 0, 0).withAlpha(0.0f);
        const Colour Accent = Colour(0xD8, 0xD8, 0xD8);
        const Colour AccentDim = Colour(0x2A, 0x2A, 0x2A);
        const Colour Body = Colour(0x0A, 0x0A, 0x0A);
        const Colour Card = Colour(0x12, 0x12, 0x12);
        const Colour CardDark = Colour(0x07, 0x07, 0x07);
        const Colour Background = Colour(0x02, 0x02, 0x02);
        const Colour BackgroundGradientStart = Colour(0x1A, 0x1A, 0x1A).withAlpha(0.9f);
        const Colour BackgroundGradientEnd = BackgroundGradientStart.withAlpha(0.0f);
        const Colour BackgroundDark = Colour(0x07, 0x07, 0x07);
        const Colour Logo = Colour(0xE0, 0xE0, 0xE0);
        const Colour BrowserBackground = Colour(0x07, 0x07, 0x07);
        const Colour KeyDown = Colour(0xFF, 0xFF, 0xFF);
    }

    namespace Layout
    {
        const float MainMargin = 12.0f;
        const float MainGap = 12.0f;
        const float CardGap = 12.0f;

        const float CardInset = 10.0f;
        const float ContentInset = 12.0f;
        const float CardCorner = 4.0f;
        const float InnerCardCorner = 4.0f;

        const float GenerationSectionHeight = 160.0f;
        const float EnvelopeSectionHeight = 166.0f;
    }

    namespace Paint
    {
        enum BorderType
        {
            Normal = 0,
            Glass
        };

        inline void drawBorders(Graphics &g,
                                Rectangle<int> componentBounds,
                                BorderType border_type = BorderType::Normal)
        {
            const float line_thickness = 1.0f;

            Point<int> topLeft = componentBounds.getTopLeft();
            Point<int> topRight = componentBounds.getTopRight();
            Point<int> bottomLeft = componentBounds.getBottomLeft();
            Point<int> bottomRight = componentBounds.getBottomRight();

            switch (border_type)
            {
            case BorderType::Normal:
                g.setColour(GUI::Color::Accent.withAlpha(0.08f));
                break;
            case BorderType::Glass:
                g.setColour(GUI::Color::Accent.withAlpha(0.10f));
                break;
            default:
                jassertfalse;
                break;
            }

            g.drawLine ((float) bottomLeft.getX(), (float) bottomLeft.getY(),
                        (float) topLeft.getX(), (float) topLeft.getY(), line_thickness);

            g.drawLine ((float) topLeft.getX(), (float) topLeft.getY(),
                        (float) topRight.getX(), (float) topRight.getY(), line_thickness);

            g.setColour (Colour (0, 0, 0).withAlpha (0.5f));

            g.drawLine ((float) topRight.getX(), (float) topRight.getY(),
                        (float) bottomRight.getX(), (float) bottomRight.getY(), line_thickness);

            g.drawLine ((float) bottomRight.getX(), (float) bottomRight.getY(),
                        (float) bottomLeft.getX(), (float) bottomLeft.getY(), line_thickness);
        }

        inline float cardOutlineThickness() noexcept
        {
            return 1.0f * BorealZoom::uiScale;
        }

        inline juce::Rectangle<float> insetCardBounds (juce::Rectangle<float> bounds) noexcept
        {
            return bounds.reduced (cardOutlineThickness() * 0.5f);
        }

        inline void drawCardOutline(Graphics &g,
                                    Rectangle<float> bounds,
                                    float corner,
                                    float alpha = 0.40f,
                                    float thicknessU = 1.0f)
        {
            const float t = thicknessU * BorealZoom::uiScale;
            g.setColour(GUI::Color::Accent.withAlpha(alpha));
            g.drawRoundedRectangle(bounds.reduced (t * 0.5f), corner, t);
        }

        inline void drawDisabled(Graphics &g, Rectangle<int>)
        {
            g.fillAll(Colours::black.withAlpha(0.25f));
        }
    }
}
