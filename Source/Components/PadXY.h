#pragma once

#include <JuceHeader.h>
#include <memory>

#include "../Helpers/CustomLookAndFeel.h"
#include "../Helpers/InterfaceDefines.h"
#include "../Helpers/BorealParameters.h"
#include "../Managers/BorealPresetManager.h"
#include "../PluginProcessor.h"

class PadXY : public Component, public Timer
{
public:
    PadXY (BorealAudioProcessor* inProcessor,
           AudioProcessorValueTreeState& stateToControl,
           Boreal::Parameter<float> freqs_interp_factor_parameter,
           Boreal::Parameter<float> mags_interp_factor_parameter)
    : circle(), mProcessor (inProcessor)
    {
        const String& x_parameterID    = freqs_interp_factor_parameter.ID;
        const String& x_parameterLabel = freqs_interp_factor_parameter.label;
        const String& y_parameterID    = mags_interp_factor_parameter.ID;
        const String& y_parameterLabel = mags_interp_factor_parameter.label;

        setSize (getWidth(), getHeight());

        x_min = 0; x_max = 1;
        y_min = 0; y_max = 1;
        x_val = (x_min + x_max) / 2;
        y_val = (x_min + x_max) / 2;

        invert_y = true;

        circle.setColour (GUI::Color::Accent.overlaidWith (Colours::white.withAlpha (0.15f)));
        circle.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (circle);

        x_axis_slider = std::make_unique<Slider> (x_parameterLabel);
        y_axis_slider = std::make_unique<Slider> (y_parameterLabel);

        x_axis_slider_attachment =
        std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (stateToControl, x_parameterID, *x_axis_slider);
        y_axis_slider_attachment =
        std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (stateToControl, y_parameterID, *y_axis_slider);

        x_axis_slider->setRange (x_min, x_max);
        x_axis_slider->setName (getName() + "_x");
        y_axis_slider->setRange (y_min, y_max);
        y_axis_slider->setName (getName() + "_y");

        startTimer (XYPAD_UI_REFRESH_TIMER_CALLBACK);
    }

    ~PadXY() {}

    void paint (Graphics& g) override
    {
        int borderWidth = 0;

        rectangle.setPosition (static_cast<float> (borderWidth) / 2.0f, static_cast<float> (borderWidth) / 2.0f);
        rectangle.setSize (static_cast<float> (getWidth() - borderWidth), static_cast<float> (getHeight() - borderWidth));
        g.setColour (Colours::transparentBlack);
        g.fillRect (rectangle);

        const float centre_x = getWidth() * 0.5f;
        const float centre_y = getHeight() * 0.5f;

        g.setColour (Colours::white.withAlpha (0.14f));
        g.drawLine (0.0f, centre_y, (float) getWidth(), centre_y, 1.0f);
        g.drawLine (centre_x, 0.0f, centre_x, (float) getHeight(), 1.0f);

        const float padRadius = jmin (getWidth(), getHeight()) * 0.5f;
        const auto ringColour = Colours::white.withAlpha (0.10f);

        for (int ring = 1; ring <= 3; ++ring)
        {
            const float r = padRadius * (float) ring / 3.0f;
            g.setColour (ringColour);
            g.drawEllipse (centre_x - r, centre_y - r, r * 2.0f, r * 2.0f, 1.0f);
        }

        g.setColour (Colours::white.withAlpha (0.10f));
        g.fillEllipse (centre_x - 2.0f, centre_y - 2.0f, 4.0f, 4.0f);

        const float zs = BorealZoom::uiScale;
        const float labelInset = 8.0f * zs;
        const float labelW = 72.0f * zs;
        const float labelH = 20.0f * zs;
        const float dotSize = 5.0f * zs;
        const float dotGap = 6.0f * zs;

        g.setFont (CustomLookAndFeel::makeFont (21.0f));

        auto drawCornerLabel = [&] (int cornerIndex, const String& text,
                                    const Rectangle<float>& textRect, Justification justification)
        {
            const bool loaded = cornerLoaded[cornerIndex];
            g.setColour (loaded ? GUI::Color::KeyDown
                                : GUI::Color::Logo);
            g.drawText (text, textRect, justification, false);

            if (loaded)
            {
                const bool isTopCorner = (cornerIndex < 2);
                const bool isLeftCorner = (cornerIndex == 0 || cornerIndex == 2);
                const float dotX = isLeftCorner ? textRect.getX()
                                                : textRect.getRight() - dotSize;
                const float dotY = isTopCorner ? textRect.getBottom() + dotGap
                                               : textRect.getY() - dotGap - dotSize;
                g.setColour (GUI::Color::KeyDown);
                g.fillRect (dotX, dotY, dotSize, dotSize);
            }
        };

        drawCornerLabel (0, "[ LH ]", { rectangle.getX() + labelInset, rectangle.getY() + labelInset, labelW, labelH }, Justification::topLeft);
        drawCornerLabel (1, "[ RH ]", { rectangle.getRight() - labelW - labelInset, rectangle.getY() + labelInset, labelW, labelH }, Justification::topRight);
        drawCornerLabel (2, "[ LL ]", { rectangle.getX() + labelInset, rectangle.getBottom() - labelH - labelInset, labelW, labelH }, Justification::bottomLeft);
        drawCornerLabel (3, "[ RL ]", { rectangle.getRight() - labelW - labelInset, rectangle.getBottom() - labelH - labelInset, labelW, labelH }, Justification::bottomRight);

        GUI::Paint::drawBorders (g, getLocalBounds(), GUI::Paint::BorderType::Glass);
    }

    void resized() override
    {

        rectangle.setWidth (static_cast<float> (getWidth()) * .98f);
        rectangle.setTop (static_cast<float> (getHeight()) * .02f);
        rectangle.setHeight (static_cast<float> (getHeight()) * .96f);
        rectangle.setLeft (static_cast<float> (getWidth()) * .02f);

        float circle_diameter = static_cast<float> (getWidth()) * 0.035f;
        circle.setSize (juce::roundToInt (circle_diameter), juce::roundToInt (circle_diameter));
        const Point<int> pos (getValueAsPosition (x_val, y_val));
        circle.setTopLeftPosition (pos.getX(), pos.getY());
    }

    void setValues (float x, float y, bool = true)
    {
        x_axis_slider->setValue (x, sendNotification);
        y_axis_slider->setValue (y, sendNotification);
    }

    class PadCircle : public Component
    {
        Point<float> circleXY;
        Colour colour;

    public:

        PadCircle() {}

        void setColour (Colour col)
        {
            colour = col;
            repaint();
        }

        void paint (Graphics& g)  override
        {
            g.setColour (colour);
            g.fillEllipse (getLocalBounds().toFloat());
        }
    };

    PadCircle* getCircle()
    {
        return &circle;
    }

private:

    Point<int> constrainPosition (float x, float y)
    {
        const float xPos = jlimit (rectangle.getX(), (rectangle.getWidth() + rectangle.getX()) - circle.getWidth(), x - circle.getWidth() / 2.f);
        const float yPos = jlimit (rectangle.getY(), (rectangle.getHeight() + rectangle.getY()) - circle.getHeight(), y - circle.getHeight() / 2.f);
        return Point<int> (juce::roundToInt (xPos), juce::roundToInt (yPos));
    }

    Point<int> getValueAsPosition (float x, float y)
    {
        if (invert_y) y = y_max - y;

        const float xPos = jmap (x, rectangle.getX(), (rectangle.getWidth() + rectangle.getX()) - circle.getWidth());
        const float yPos = jmap (y, rectangle.getY(), (rectangle.getHeight() + rectangle.getY()) - circle.getHeight());
        return Point<int> (juce::roundToInt (xPos), juce::roundToInt (yPos));
    }

    void setPositionAsValue (Point<float> position)
    {
        const float xVal = jlimit (x_min, x_max, jmap (position.getX(), rectangle.getX(), rectangle.getWidth() - circle.getWidth(), x_min, x_max));
        float yVal = jlimit (y_min, y_max, jmap (position.getY(), rectangle.getY(), rectangle.getHeight() - circle.getHeight(), y_min, y_max));

        if (invert_y) yVal = y_max - yVal;

        setValues (xVal, yVal);
    }

    void mouseDown (const MouseEvent& e) override
    {
        circle.setTopLeftPosition (constrainPosition (static_cast<float> (e.getPosition().getX()), static_cast<float> (e.getPosition().getY())));
        mouseDownXY.setXY (circle.getPosition().getX() + circle.getWidth()*.5f, circle.getPosition().getY() + circle.getHeight()*.5f);
        setPositionAsValue (circle.getPosition().toFloat());
        repaint();
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (e.mouseWasDraggedSinceMouseDown())
        {
            circle.setTopLeftPosition (constrainPosition (mouseDownXY.getX() + e.getDistanceFromDragStartX(), mouseDownXY.getY() + e.getDistanceFromDragStartY()));
            setPositionAsValue (circle.getPosition().toFloat());
            repaint();

            currentMouseXY = circle.getPosition().toFloat();
        }
    }

    void timerCallback() override
    {

        const Point<int> target_position (getValueAsPosition (static_cast<float> (x_axis_slider->getValue()),
                                                              static_cast<float> (y_axis_slider->getValue())));

        const Point<int> current_position = circle.getPosition();

        if (current_position != target_position)
        {
            const float smoothing = 0.35f;
            const float next_x = current_position.getX() + (target_position.getX() - current_position.getX()) * smoothing;
            const float next_y = current_position.getY() + (target_position.getY() - current_position.getY()) * smoothing;

            circle.setTopLeftPosition (roundToInt (next_x), roundToInt (next_y));
            repaint();
        }

        if (mCurrentPresetName != mProcessor->getPresetManager()->getCurrentPresetName())
        {

            mCurrentPresetName = mProcessor->getPresetManager()->getCurrentPresetName();
            repaint();
        }

        const int mask = mProcessor->getSlotLoadMask();
        const bool loaded[4] = { (mask & 1) != 0, (mask & 2) != 0,
                                 (mask & 4) != 0, (mask & 8) != 0 };
        bool changed = false;
        for (int i = 0; i < 4; ++i)
        {
            if (loaded[i] != cornerLoaded[i])
            {
                cornerLoaded[i] = loaded[i];
                changed = true;
            }
        }
        if (changed)
            repaint();
    }

    int mParameterID;

    PadCircle circle;
    Rectangle<float> rectangle;

    float x_min, x_max, y_min, y_max, x_val, y_val;
    bool invert_y;

    bool cornerLoaded[4] = { false, false, false, false };

    Point<float> currentMouseXY;
    Point<float> mouseDownXY;

    std::unique_ptr<Slider> x_axis_slider;
    std::unique_ptr<Slider> y_axis_slider;

    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> x_axis_slider_attachment;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> y_axis_slider_attachment;

    String mCurrentPresetName;

    BorealAudioProcessor* mProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadXY)
};
