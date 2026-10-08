#pragma once
#include <JuceHeader.h>

#include "BorealConstants.h"

namespace BorealColors
{

    static const juce::Colour background{0xff020202};
    static const juce::Colour body{0xff0A0A0A};
    static const juce::Colour card{0xff121212};
    static const juce::Colour cardDark{0xff070707};
    static const juce::Colour headerBg{0xff020202};

    static const juce::Colour accent{0xffd8d8d8};
    static const juce::Colour highlight{0xffffffff};
    static const juce::Colour accentDim{0xff2a2a2a};
    static const juce::Colour accentSoft{0xff6a6a6a};

    static const juce::Colour textPrimary{0xffd8d8d8};
    static const juce::Colour textMid{0xff8a8a8a};
    static const juce::Colour textBrand{0xffffffff};

    static const juce::Colour buttonOff{0xff101010};
    static const juce::Colour buttonOn{0xff262626};
    static const juce::Colour buttonBorder{0xff383838};

    static const juce::Colour menuBg{0xff0c0c0c};
    static const juce::Colour menuText{0xffd8d8d8};
    static const juce::Colour menuTextBright{0xffffffff};
    static const juce::Colour menuTextDim{0xff6a6a6a};
    static const juce::Colour menuHover{0xff2e2e2e};
    static const juce::Colour menuBorder{0xff3a3a3a};
    static const juce::Colour menuInnerBorder{0xff1a1a1a};
}

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:

    struct TooltipAnchor
    {
        static inline juce::Point<int> pos {};
        static inline bool valid = false;
    };
    static const juce::Typeface::Ptr& getTypeface()
    {
        static const juce::Typeface::Ptr typeface = juce::Typeface::createSystemTypefaceFor(
            BinaryData::VT323Regular_ttf,
            BinaryData::VT323Regular_ttfSize);
        return typeface;
    }

   #if JUCE_LINUX
    static constexpr float kFontSizeScale = 0.87f;
   #else
    static constexpr float kFontSizeScale = 1.0f;
   #endif

    static juce::Font makeFont (float height)
    {
        return juce::Font (juce::FontOptions (getTypeface())
                               .withMetricsKind (juce::TypefaceMetricsKind::legacy))
            .withHeight (height * kFontSizeScale * BorealZoom::uiScale);
    }

    void setScale (float s) noexcept { BorealZoom::uiScale = s; }
    float getScale() const noexcept { return BorealZoom::uiScale; }

    CustomLookAndFeel()
    {
        using namespace BorealColors;
        setColour(juce::ResizableWindow::backgroundColourId, background);

        setColour(juce::Slider::thumbColourId, accent);
        setColour(juce::Slider::trackColourId, accentDim);
        setColour(juce::Slider::rotarySliderFillColourId, highlight);
        setColour(juce::Slider::rotarySliderOutlineColourId, accentDim);
        setColour(juce::Slider::textBoxTextColourId, textPrimary);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxHighlightColourId, accentDim);

        setColour(juce::ScrollBar::thumbColourId, accentDim);

        setColour(juce::ComboBox::backgroundColourId, buttonOff);
        setColour(juce::ComboBox::outlineColourId, buttonBorder);
        setColour(juce::ComboBox::textColourId, textPrimary);
        setColour(juce::ComboBox::arrowColourId, textMid);

        setColour(juce::TextButton::buttonColourId, buttonOff);
        setColour(juce::TextButton::buttonOnColourId, buttonOn);
        setColour(juce::TextButton::textColourOffId, textMid);
        setColour(juce::TextButton::textColourOnId, accent);

        setColour(juce::Label::textColourId, textMid);

        setColour(juce::PopupMenu::backgroundColourId, menuBg);
        setColour(juce::PopupMenu::textColourId, menuText);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, menuHover);
        setColour(juce::PopupMenu::highlightedTextColourId, menuTextBright);

        setColour(juce::TooltipWindow::backgroundColourId, menuBg);
        setColour(juce::TooltipWindow::textColourId, menuText);
        setColour(juce::TooltipWindow::outlineColourId, menuBorder);

        setColour(juce::AlertWindow::backgroundColourId, background);
        setColour(juce::AlertWindow::textColourId, menuText);
        setColour(juce::AlertWindow::outlineColourId, menuBorder);

        setColour(juce::TextEditor::backgroundColourId, menuBg);
        setColour(juce::TextEditor::textColourId, menuTextBright);
        setColour(juce::TextEditor::highlightColourId, menuHover);
        setColour(juce::TextEditor::highlightedTextColourId, menuTextBright);
        setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        setColour(juce::TextEditor::focusedOutlineColourId, menuBorder);
        setColour(juce::Label::textWhenEditingColourId, menuTextBright);
        setColour(juce::Label::backgroundWhenEditingColourId, menuBg);
        setColour(juce::Label::outlineWhenEditingColourId, menuBorder);
        setColour(juce::CaretComponent::caretColourId, menuTextBright);

        setColour(juce::DocumentWindow::textColourId, textBrand);
        setColour(juce::ListBox::backgroundColourId, body);
        setColour(juce::ListBox::textColourId, textPrimary);
        setColour(juce::ListBox::outlineColourId, menuBorder);
        setColour(juce::TableHeaderComponent::backgroundColourId, headerBg);
        setColour(juce::TableHeaderComponent::textColourId, textMid);
        setColour(juce::TableHeaderComponent::highlightColourId, buttonOn);
        setColour(juce::GroupComponent::outlineColourId, accentDim);
        setColour(juce::GroupComponent::textColourId, textMid);
        setColour(juce::ToggleButton::textColourId, textPrimary);
        setColour(juce::ProgressBar::foregroundColourId, accent);
        setColour(juce::ProgressBar::backgroundColourId, accentDim);
    }

    juce::Font getFont(float height, bool  = true) const
    {
        if (getTypeface() != nullptr)
            return makeFont (height);
        return juce::Font(juce::FontOptions (juce::Font::getDefaultSansSerifFontName(),
                                                 height * kFontSizeScale * BorealZoom::uiScale, juce::Font::plain)
                               .withMetricsKind (juce::TypefaceMetricsKind::legacy));
    }

    juce::Font getCustomFont(float height, bool bold = true) const
    {
        return getFont(height, bold);
    }

    juce::Font getLabelFont(juce::Label &label) override
    {
        return label.getFont();
    }

    juce::Font getComboBoxFont(juce::ComboBox &) override
    {
        return getFont(24.0f);
    }

    juce::Font getTextButtonFont(juce::TextButton &, int height) override
    {
        return getFont(jmin(24.0f, (float) height * 0.85f));
    }

    juce::Font getAlertWindowTitleFont() override { return getFont(36.0f); }
    juce::Font getAlertWindowMessageFont() override { return getFont(26.0f, false); }
    int getAlertWindowButtonHeight() override { return juce::roundToInt (34.0f * BorealZoom::uiScale); }

    juce::Font getPopupMenuFont() override
    {
        return getCustomFont (24.0f);
    }
    int getPopupMenuBorderSize() override { return juce::roundToInt (3.0f * BorealZoom::uiScale); }

    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override
    {
        const juce::TextLayout tl (layoutTooltipText (tipText, findColour (juce::TooltipWindow::textColourId)));

        const int w = (int) (tl.getWidth() + 14.0f * BorealZoom::uiScale);
        const int h = (int) (tl.getHeight() + 6.0f * BorealZoom::uiScale);
        const int gap = juce::roundToInt (10.0f * BorealZoom::uiScale);

        if (TooltipAnchor::valid)
        {
            const int x = TooltipAnchor::pos.x - w / 2;
            int y = TooltipAnchor::pos.y - h - gap;
            if (y < parentArea.getY())
                y = TooltipAnchor::pos.y + gap;
            return juce::Rectangle<int> (x, y, w, h).constrainedWithin (parentArea);
        }

        return juce::Rectangle<int> (screenPos.x + 20, screenPos.y + 16, w, h)
                 .constrainedWithin (parentArea);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override
    {
        juce::Rectangle<int> bounds (width, height);
        const float cornerSize = 5.0f * BorealZoom::uiScale;

        g.setColour (findColour (juce::TooltipWindow::backgroundColourId));
        g.fillRoundedRectangle (bounds.toFloat(), cornerSize);

        g.setColour (findColour (juce::TooltipWindow::outlineColourId));
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f, 0.5f), cornerSize, 1.0f);

        layoutTooltipText (text, findColour (juce::TooltipWindow::textColourId))
            .draw (g, { static_cast<float> (width), static_cast<float> (height) });
    }

    void drawPopupMenuBackground(juce::Graphics &g, int width, int height) override
    {
        using namespace BorealColors;

        g.fillAll(menuBg);

        g.setColour(Colours::black.withAlpha(0.25f));
        for (int y = 0; y < height; y += 2)
            g.fillRect(0, y, width, 1);

        g.setColour(menuBorder.withAlpha(0.7f));
        g.drawRect(0, 0, width, height, 1);
        g.setColour(menuInnerBorder);
        g.drawRect(1, 1, width - 2, height - 2, 1);
    }

    void drawPopupMenuItem(juce::Graphics &g,
                           const juce::Rectangle<int> &area,
                           bool isSeparator,
                           bool isActive,
                           bool isHighlighted,
                           bool,
                           bool,
                           const juce::String &text,
                           const juce::String &,
                           const juce::Drawable *,
                           const juce::Colour *) override
    {
        using namespace BorealColors;

        if (isSeparator)
        {
            g.setColour(menuBorder.withAlpha(0.6f));
            g.fillRect(area.getX(), area.getCentreY(), area.getWidth(), 1);
            return;
        }

        if (isHighlighted)
        {
            g.setColour(menuHover);
            g.fillRect(area.getX(), area.getY() + 1, area.getWidth(), area.getHeight() - 1);
            g.setColour(menuTextBright.withAlpha(0.8f));
            g.fillRect(area.getX(), area.getY() + 1, 2, area.getHeight() - 1);
        }

        g.setColour(Colours::black.withAlpha(0.22f));
        for (int y = 1; y < area.getHeight(); y += 2)
            g.fillRect(area.getX(), area.getY() + y, area.getWidth(), 1);

        g.setColour(! isActive   ? menuTextDim
                    : isHighlighted ? menuTextBright
                                    : menuText);
        g.setFont(getPopupMenuFont());
        g.drawText(text, area.reduced(10, 0), juce::Justification::centredLeft, true);
    }

    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                    int standardMenuItemHeight,
                                    int& idealWidth, int& idealHeight) override
    {
        LookAndFeel_V4::getIdealPopupMenuItemSize (text, isSeparator,
                                                   standardMenuItemHeight,
                                                   idealWidth, idealHeight);
        idealWidth = jmax (idealWidth, juce::roundToInt (220.0f * BorealZoom::uiScale));
        idealHeight = juce::roundToInt ((float) idealHeight * BorealZoom::uiScale);
    }

    juce::Slider::SliderLayout getSliderLayout(juce::Slider &slider) override
    {
        juce::Slider::SliderLayout layout;
        auto bounds = slider.getLocalBounds();

        const float s = BorealZoom::uiScale;
        if (slider.isRotary())
        {
            layout.textBoxBounds = bounds.removeFromBottom (juce::roundToInt (30.0f * s))
                                       .removeFromRight (juce::roundToInt (96.0f * s))
                                       .translated (juce::roundToInt (-8.0f * s), juce::roundToInt (-4.0f * s));
            layout.sliderBounds = bounds.expanded (juce::roundToInt (4.0f * s), juce::roundToInt (4.0f * s))
                                       .translated (0, juce::roundToInt (3.0f * s));
        }
        else if (slider.isHorizontal())
        {
            auto valueBox = bounds.removeFromRight (juce::roundToInt (96.0f * s));
            layout.textBoxBounds = valueBox.withSizeKeepingCentre (valueBox.getWidth(),
                                                                   juce::roundToInt (24.0f * s))
                                           .translated (0, -2);
            layout.sliderBounds = bounds.reduced (juce::roundToInt (4.0f * s), 0);
        }
        else
        {
            layout.textBoxBounds = bounds.removeFromBottom (juce::roundToInt (60.0f * s));
            layout.sliderBounds = bounds.reduced (0, juce::roundToInt (8.0f * s)).translated (0, juce::roundToInt (8.0f * s));
        }
        return layout;
    }

    void drawRotarySlider(juce::Graphics &g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider &) override
    {
        using namespace BorealColors;

        const float cx = (float)x + (float)width * 0.5f;
        const float cy = (float)y + (float)height * 0.54f;
        const float s = BorealZoom::uiScale;
        const float r = juce::jmin((float)width, (float)height) * 0.5f - 26.0f * s;
        const float toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const float arcR = r + 6.0f * s;

        juce::Path bgArc;
        bgArc.addCentredArc(cx, cy, arcR, arcR, 0.0f,
                            rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(accentDim);
        g.strokePath(bgArc, juce::PathStrokeType(3.0f * s,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::square));

        juce::Path valArc;
        valArc.addCentredArc(cx, cy, arcR, arcR, 0.0f,
                             rotaryStartAngle, toAngle, true);
        g.setColour(highlight);
        g.strokePath(valArc, juce::PathStrokeType(3.0f * s,
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::square));

        g.setColour(cardDark);
        g.fillRoundedRectangle(cx - r, cy - r, r * 2.0f, r * 2.0f, 2.0f);

        const float outerR = r * 0.85f;
        g.setColour(accent);
        const float pLen = 6.0f * s;
        const float pWid = 3.0f * s;
        juce::Path pointer;
        pointer.addRectangle(cx - pWid * 0.5f, cy - outerR - pLen,
                             pWid, pLen);
        pointer.applyTransform(juce::AffineTransform::rotation(toAngle, cx, cy));
        g.fillPath(pointer);
    }

    void drawLinearSlider(juce::Graphics &g,
                          int x, int y, int width, int height,
                          float sliderPos, float, float,
                          const juce::Slider::SliderStyle style,
                          juce::Slider &) override
    {
        using namespace BorealColors;

        if (style == juce::Slider::LinearHorizontal)
        {

            const float th = 12.0f * BorealZoom::uiScale;
            juce::Rectangle<float> track{
                (float)x, (float)y + (float)height * 0.5f - th * 0.5f,
                (float)width, th};

            g.setColour(accentDim);
            g.fillRect(track);

            const float fillW = sliderPos - track.getX();
            if (fillW > 0.5f)
            {
                g.setColour(highlight);
                g.fillRect(juce::Rectangle<float>(track.getX(), track.getY(), fillW, th));
            }
            return;
        }

        const float tw = 12.0f * BorealZoom::uiScale;

        juce::Rectangle<float> track{
            (float)x + (float)width * 0.5f - tw * 0.5f,
            (float)y, tw, (float)height};

        g.setColour(accentDim);
        g.fillRect(track);

        float fillH = track.getBottom() - sliderPos;

        if (fillH > 0.5f)
        {
            float fillY = track.getBottom() - fillH;

            g.setColour(highlight);
            g.fillRect(juce::Rectangle<float>(track.getX(), fillY, tw, fillH));
        }
    }

    void drawButtonBackground(juce::Graphics &g, juce::Button &button,
                              const juce::Colour &,
                              bool shouldDrawButtonAsHighlighted,
                              bool) override
    {
        using namespace BorealColors;

        const float s = BorealZoom::uiScale;
        const bool isOn = button.getToggleState();
        const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f * s);

        if (isOn)
        {
            g.setColour(buttonOn);
            g.fillRoundedRectangle(bounds, 2.0f * s);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            g.setColour(buttonOn.withAlpha(0.6f));
            g.fillRoundedRectangle(bounds, 2.0f * s);
        }
        else
        {
            g.setColour(buttonOff);
            g.fillRoundedRectangle(bounds, 2.0f * s);
        }

        g.setColour(buttonBorder);
        g.drawRoundedRectangle(bounds, 2.0f * s, 1.0f);
    }

    void drawButtonText(juce::Graphics &g, juce::TextButton &button,
                        bool shouldDrawButtonAsHighlighted, bool) override
    {
        using namespace BorealColors;

        const bool isOn = button.getToggleState();
        const auto area = button.getLocalBounds().toFloat();

        if (button.getButtonText() == "X")
        {
            g.setFont(getCustomFont(18.0f));
            g.setColour(shouldDrawButtonAsHighlighted ? textBrand : textMid);
            g.drawText("X", area.translated(0, -2.0f * BorealZoom::uiScale), juce::Justification::centred, false);
            return;
        }

        g.setFont(getTextButtonFont(button, button.getHeight()));
        g.setColour(isOn ? textPrimary : (shouldDrawButtonAsHighlighted ? textPrimary : textMid));

        const juce::String text = isOn ? ("> " + button.getButtonText() + " <")
                                       : ("[ " + button.getButtonText() + " ]");

        g.drawText(text, area.translated(0.0f, -2.0f * BorealZoom::uiScale), juce::Justification::centred, true);
    }

     void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                            bool shouldDrawButtonAsHighlighted, bool isButtonDown) override
     {

         if (!button.getButtonText().isEmpty())
         {
             drawButtonBackground(g, button, findColour(juce::TextButton::buttonColourId),
                                  shouldDrawButtonAsHighlighted, isButtonDown);

             using namespace BorealColors;
             const bool isOn = button.getToggleState();
             const auto area = button.getLocalBounds();
             g.setFont(getCustomFont(24.0f));
             g.setColour(isOn ? textPrimary : (shouldDrawButtonAsHighlighted ? textPrimary : textMid));

              const juce::String text = isOn ? ("> " + button.getButtonText() + " <")
                                             : ("[ " + button.getButtonText() + " ]");

              g.drawText(text, area.translated(0, juce::roundToInt (-2.0f * BorealZoom::uiScale)), juce::Justification::centred, true);
         }
         else
         {
             LookAndFeel_V4::drawToggleButton (g, button, shouldDrawButtonAsHighlighted, isButtonDown);
         }
     }

    void drawComboBox(juce::Graphics &g,
                      int width, int height,
                      bool, int, int, int, int,
                      juce::ComboBox& box) override
    {
        using namespace BorealColors;

        const float s = BorealZoom::uiScale;
        g.setColour(buttonOff);
        g.fillRoundedRectangle(0, 0, (float)width, (float)height, 2.0f * s);
        g.setColour(buttonBorder);
        g.drawRoundedRectangle(0.5f, 0.5f,
                               (float)width - 1.0f, (float)height - 1.0f, 2.0f * s, 1.0f);

        if (box.getTextWhenNothingSelected() == "\xe2\x98\xb0")
            return;

        juce::Path p;
        p.addTriangle((float)width - 14.0f * s, (float)height * 0.5f - 2.0f * s,
                      (float)width - 6.0f * s, (float)height * 0.5f - 2.0f * s,
                      (float)width - 10.0f * s, (float)height * 0.5f + 3.0f * s);
        g.setColour(textMid);
        g.fillPath(p);
    }

    void positionComboBoxText(juce::ComboBox &box, juce::Label &label) override
    {

        const float s = BorealZoom::uiScale;
        label.setBounds(0, 0, box.getWidth() - juce::roundToInt (20.0f * s), box.getHeight());
        label.setFont(getCustomFont (juce::jmin (20.0f, (float) box.getHeight() - 4.0f * s)));
        label.setJustificationType(juce::Justification::centredLeft);
        label.setColour(juce::Label::textColourId, BorealColors::textPrimary);
    }

    void drawComboBoxTextWhenNothingSelected(juce::Graphics &g, juce::ComboBox &box,
                                             juce::Label &label) override
    {

        if (box.getTextWhenNothingSelected() == "\xe2\x98\xb0")
        {
            const auto r = box.getLocalBounds().toFloat();
            const float cx = r.getCentreX();
            const float cy = r.getCentreY();

            const float barW = 14.0f * BorealZoom::uiScale;
            const float barH = 2.0f * BorealZoom::uiScale;
            const float gap  = 5.0f * BorealZoom::uiScale;

            g.setColour (BorealColors::textMid);
            g.fillRect (juce::Rectangle<float> (cx - barW * 0.5f, cy - gap - barH * 0.5f, barW, barH));
            g.fillRect (juce::Rectangle<float> (cx - barW * 0.5f, cy - barH * 0.5f,          barW, barH));
            g.fillRect (juce::Rectangle<float> (cx - barW * 0.5f, cy + gap - barH * 0.5f,    barW, barH));
            return;
        }

        juce::LookAndFeel_V4::drawComboBoxTextWhenNothingSelected (g, box, label);
    }

    void drawTreeviewPlusMinusBox(juce::Graphics &g, const juce::Rectangle<float> &area,
                                  juce::Colour, bool isOpen, bool) override
    {
        using namespace BorealColors;

        juce::Path arrow;

        if (isOpen)
        {
            arrow.addTriangle(area.getX() + area.getWidth() * 0.2f, area.getY() + area.getHeight() * 0.3f,
                              area.getX() + area.getWidth() * 0.8f, area.getY() + area.getHeight() * 0.3f,
                              area.getCentreX(),                     area.getY() + area.getHeight() * 0.7f);
        }
        else
        {
            arrow.addTriangle(area.getX() + area.getWidth() * 0.3f, area.getY() + area.getHeight() * 0.2f,
                              area.getX() + area.getWidth() * 0.3f, area.getY() + area.getHeight() * 0.8f,
                              area.getX() + area.getWidth() * 0.7f,  area.getCentreY());
        }

        g.setColour(textMid);
        g.fillPath(arrow);
    }

    void drawFileBrowserRow(juce::Graphics &g, int width, int height,
                            const juce::File &, const juce::String &filename, juce::Image *icon,
                            const juce::String &fileSizeDescription,
                            const juce::String &fileTimeDescription,
                            bool isDirectory, bool isItemSelected,
                            int, juce::DirectoryContentsDisplayComponent &) override
    {
        using namespace BorealColors;

        if (isItemSelected)
        {
            g.setColour(buttonOn);
            g.fillAll();
            g.setColour(accentSoft);
            g.fillRect(0, 0, 2, height);
        }

        const float s = BorealZoom::uiScale;
        if (icon != nullptr && icon->isValid())
        {
            g.drawImageWithin(*icon, 2, 2, juce::roundToInt (24.0f * s), height - juce::roundToInt (4.0f * s),
                              juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize,
                              false);
        }

        g.setColour(isDirectory ? textBrand : textPrimary);
        g.setFont(getFont(juce::jmin(26.0f, (float) height * 0.78f)));
        g.drawFittedText(filename, juce::roundToInt (28.0f * s), 0, width - juce::roundToInt (30.0f * s), height, juce::Justification::centredLeft, 1);

        if (width > 450 && ! isDirectory)
        {
            auto sizeX = juce::roundToInt((float) width * 0.7f);
            auto dateX = juce::roundToInt((float) width * 0.82f);

            g.setFont(getFont(22.0f, false));
            g.setColour(textMid);
            g.drawFittedText(fileSizeDescription, sizeX, 0, dateX - sizeX - 8, height, juce::Justification::centredRight, 1);
            g.drawFittedText(fileTimeDescription, dateX, 0, width - 8 - dateX, height, juce::Justification::centredRight, 1);
        }
    }

    void layoutFileBrowserComponent(juce::FileBrowserComponent &browserComp,
                                    juce::DirectoryContentsDisplayComponent *fileListComponent,
                                    juce::FilePreviewComponent *previewComp,
                                    juce::ComboBox *currentPathBox,
                                    juce::TextEditor *filenameBox,
                                    juce::Button *goUpButton) override
    {

        const float s = BorealZoom::uiScale;
        auto b = browserComp.getLocalBounds().reduced (juce::roundToInt (26.0f * s), juce::roundToInt (20.0f * s));
        b.removeFromBottom (juce::roundToInt (2.0f * s));

        auto topSlice    = b.removeFromTop (juce::jmin (juce::roundToInt (24.0f * s), b.getHeight()));
        auto bottomSlice = b.removeFromBottom (juce::jmin (juce::roundToInt (26.0f * s), b.getHeight()));

        currentPathBox->setBounds(topSlice.removeFromLeft(topSlice.getWidth() - juce::roundToInt (50.0f * s)));
        topSlice.removeFromLeft (juce::roundToInt (6.0f * s));
        goUpButton->setBounds(topSlice);

        const int labelWidth = juce::roundToInt (56.0f * s);
        auto labelArea = bottomSlice.removeFromLeft(labelWidth);
        filenameBox->setBounds(bottomSlice);

        if (filenameBox != nullptr)
        {
            filenameBox->setFont (getCustomFont (21.0f));
            filenameBox->applyFontToAllText (getCustomFont (21.0f));
            filenameBox->setJustification (juce::Justification::centredLeft);
            filenameBox->setIndents (juce::roundToInt (8.0f * s), 0);
            filenameBox->setColour (juce::TextEditor::textColourId, BorealColors::textPrimary);
            filenameBox->setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
            filenameBox->setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        }

        if (auto *fileLabel = findBrowserFileLabel(browserComp))
        {
            fileLabel->setFont(getFont(21.0f));
            fileLabel->setJustificationType(juce::Justification::centredLeft);
            fileLabel->setColour(juce::Label::textColourId, BorealColors::textMid);
            fileLabel->setBounds(labelArea);
        }

        if (previewComp != nullptr)
            previewComp->setBounds(b.removeFromRight(b.getWidth() / 3));

        if (auto *listAsComp = dynamic_cast<juce::Component *> (fileListComponent))
            listAsComp->setBounds(b.reduced(0, juce::roundToInt (6.0f * s)));
    }

    void drawAlertBox(juce::Graphics &g, juce::AlertWindow &alert,
                      const juce::Rectangle<int> &textArea,
                      juce::TextLayout &textLayout) override
    {
        using namespace BorealColors;

        const auto bounds = alert.getLocalBounds();

        g.setColour(background);
        g.fillRect(bounds);

        g.setColour(Colours::black.withAlpha(0.28f));
        for (int y = 0; y < bounds.getHeight(); y += 2)
            g.fillRect(0, y, bounds.getWidth(), 1);

        g.setColour(accent.withAlpha(0.22f));
        g.drawRect(0, 0, bounds.getWidth(), bounds.getHeight(), 1);
        g.setColour(accentDim);
        g.drawRect(1, 1, bounds.getWidth() - 2, bounds.getHeight() - 2, 1);

        const auto padded = textArea.reduced (juce::roundToInt (24.0f * BorealZoom::uiScale), juce::roundToInt (6.0f * BorealZoom::uiScale));
        if (padded.isEmpty())
            return;

        const float layoutHeight = textLayout.getHeight();
        const int drawY = padded.getY();

        textLayout.draw(g, juce::Rectangle<int>(padded.getX(), drawY,
                                                padded.getWidth(),
                                                jmin(padded.getHeight(), (int) layoutHeight)).toFloat());
    }

    class BorealDocumentWindowButton final : public juce::Button
    {
    public:
        BorealDocumentWindowButton (const juce::String& name, juce::Colour c,
                                    const juce::Path& normal, const juce::Path& toggled)
            : juce::Button (name), colour (c), normalShape (normal), toggledShape (toggled) {}

        void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
        {
            g.fillAll (BorealColors::background);

            g.setColour ((! isEnabled() || shouldDrawButtonAsDown) ? colour.withAlpha (0.6f) : colour);

            if (shouldDrawButtonAsHighlighted)
            {
                g.fillAll();
                g.setColour (BorealColors::background);
            }

            auto& p = getToggleState() ? toggledShape : normalShape;

            auto reducedRect = juce::Justification (juce::Justification::centred)
                                  .appliedToRectangle (juce::Rectangle<int> (getHeight(), getHeight()), getLocalBounds())
                                  .toFloat()
                                  .reduced ((float) getHeight() * 0.3f);

            g.fillPath (p, p.getTransformToScaleToFit (reducedRect, true));
        }

    private:
        juce::Colour colour;
        juce::Path normalShape, toggledShape;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BorealDocumentWindowButton)
    };

    juce::Button* createDocumentWindowButton (int buttonType) override
    {
        juce::Path shape;
        const auto crossThickness = 0.15f;

        if (buttonType == juce::DocumentWindow::closeButton)
        {
            shape.addLineSegment ({ 0.0f, 0.0f, 1.0f, 1.0f }, crossThickness);
            shape.addLineSegment ({ 1.0f, 0.0f, 0.0f, 1.0f }, crossThickness);
            return new BorealDocumentWindowButton ("close", BorealColors::textBrand, shape, shape);
        }

        if (buttonType == juce::DocumentWindow::minimiseButton)
        {
            shape.addLineSegment ({ 0.0f, 0.5f, 1.0f, 0.5f }, crossThickness);
            return new BorealDocumentWindowButton ("minimise", BorealColors::textBrand, shape, shape);
        }

        if (buttonType == juce::DocumentWindow::maximiseButton)
        {
            shape.addLineSegment ({ 0.5f, 0.0f, 0.5f, 1.0f }, crossThickness);
            shape.addLineSegment ({ 0.0f, 0.5f, 1.0f, 0.5f }, crossThickness);
            return new BorealDocumentWindowButton ("maximise", BorealColors::textBrand, shape, shape);
        }

        jassertfalse;
        return nullptr;
    }

    void drawDocumentWindowTitleBar (juce::DocumentWindow& window, juce::Graphics& g,
                                     int w, int h, int titleSpaceX, int titleSpaceW,
                                     const juce::Image* icon, bool drawTitleTextOnLeft) override
    {
        if (w * h == 0)
            return;

        g.fillAll (BorealColors::background);

        auto font = makeFont ((float) h * 0.65f);
        g.setFont (font);

        auto textW = juce::GlyphArrangement::getStringWidthInt (font, window.getName());
        auto iconW = 0;
        auto iconH = 0;

        if (icon != nullptr)
        {
            iconH = static_cast<int> (font.getHeight());
            iconW = icon->getWidth() * iconH / icon->getHeight() + juce::roundToInt (4.0f * BorealZoom::uiScale);
        }

        textW = juce::jmin (titleSpaceW, textW + iconW);
        auto textX = drawTitleTextOnLeft ? titleSpaceX
                                         : juce::jmax (titleSpaceX, (w - textW) / 2);

        if (textX + textW > titleSpaceX + titleSpaceW)
            textX = titleSpaceX + titleSpaceW - textW;

        if (icon != nullptr)
        {
            g.setOpacity (window.isActiveWindow() ? 1.0f : 0.6f);
            g.drawImageWithin (*icon, textX, (h - iconH) / 2, iconW, iconH,
                               juce::RectanglePlacement::centred, false);
            textX += iconW;
            textW -= iconW;
        }

        g.setColour (BorealColors::textBrand);
        g.drawText (window.getName(), textX, 0, textW, h, juce::Justification::centredLeft, true);
    }

    class BorealSliderTextLabel final : public juce::Label
    {
    public:
        BorealSliderTextLabel() : juce::Label ({}, {}) {}

        void lookAndFeelChanged() override
        {
            juce::Label::lookAndFeelChanged();
            setFont (CustomLookAndFeel::makeFont (20.0f));
        }

        void editorShown (juce::TextEditor* te) override
        {
            te->setColour (juce::TextEditor::outlineColourId,
                           BorealColors::menuBorder.withAlpha (0.35f));
            te->setColour (juce::TextEditor::backgroundColourId,
                           BorealColors::menuBg.withAlpha (0.6f));
            te->setJustification (juce::Justification::centred);
        }

        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override {}

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BorealSliderTextLabel)
    };

    static juce::TextLayout layoutTooltipText (const juce::String& text, juce::Colour colour)
    {
        const float tooltipFontSize = 15.0f * BorealZoom::uiScale;
        const int maxToolTipWidth = juce::roundToInt (400.0f * BorealZoom::uiScale);

        juce::AttributedString s;
        s.setWordWrap (juce::AttributedString::WordWrap::byChar);
        s.setJustification (juce::Justification::centred);
        s.append (text, juce::FontOptions (getTypeface()).withMetricsKind (juce::TypefaceMetricsKind::legacy).withHeight (tooltipFontSize), colour);

        juce::TextLayout tl;
        tl.createLayoutWithBalancedLineLengths (s, (float) maxToolTipWidth);
        return tl;
    }

    juce::Label* createSliderTextBox (juce::Slider &slider) override
    {
        using namespace BorealColors;

        auto *l = new BorealSliderTextLabel();
        l->setJustificationType (juce::Justification::centred);
        l->setKeyboardType (juce::TextInputTarget::decimalKeyboard);
        l->setFont (getFont (20.0f));

        l->setColour (juce::Label::textColourId, slider.findColour (juce::Slider::textBoxTextColourId));
        l->setColour (juce::Label::backgroundColourId, slider.findColour (juce::Slider::textBoxBackgroundColourId));
        l->setColour (juce::Label::outlineColourId, slider.findColour (juce::Slider::textBoxOutlineColourId));
        l->setColour (juce::TextEditor::textColourId, slider.findColour (juce::Slider::textBoxTextColourId));
        l->setColour (juce::TextEditor::backgroundColourId, slider.findColour (juce::Slider::textBoxBackgroundColourId));
        l->setColour (juce::TextEditor::outlineColourId, slider.findColour (juce::Slider::textBoxOutlineColourId));
        l->setColour (juce::TextEditor::highlightColourId, slider.findColour (juce::Slider::textBoxHighlightColourId));
        return l;
    }

private:

    static juce::Label* findBrowserFileLabel (juce::Component& comp)
    {
        for (auto* child : comp.getChildren())
        {
            if (auto* label = dynamic_cast<juce::Label*> (child))
                if (label->getText().trim().endsWithIgnoreCase ("FILE:"))
                    return label;

            if (auto* found = findBrowserFileLabel (*child))
                return found;
        }
        return nullptr;
    }

};
