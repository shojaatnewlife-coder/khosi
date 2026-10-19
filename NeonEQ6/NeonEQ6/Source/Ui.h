#pragma once
#include <JuceHeader.h>

namespace Neon
{
    inline const juce::Colour bg     { 0xff09090e };
    inline const juce::Colour panel  { 0xff0f0f17 };
    inline const juce::Colour line   { 0xff23232f };
    inline const juce::Colour text   { 0xffe9e9f2 };
    inline const juce::Colour dim    { 0xff7b7b90 };
    inline const juce::Colour green  { 0xff5cff8a };   // phosphor green
    inline const juce::Colour pink   { 0xffff6fae };   // rosy pink
    inline const juce::Colour purple { 0xffa66bff };
    inline const juce::Colour white  { 0xfff4f4fa };

    inline juce::Colour bandColour (int i)
    {
        static const juce::Colour c[6] = { green, pink, purple, white, green, pink };
        return c[(size_t) (i % 6)];
    }

    inline juce::Font font (float h, bool bold = false)
    {
        juce::Font f (juce::FontOptions (h));
        return bold ? f.boldened() : f;
    }

    inline void strokeGlow (juce::Graphics& g, const juce::Path& p, juce::Colour c, float w = 1.5f)
    {
        using PST = juce::PathStrokeType;
        g.setColour (c.withAlpha (0.05f));
        g.strokePath (p, PST (w * 7.f, PST::curved, PST::rounded));
        g.setColour (c.withAlpha (0.12f));
        g.strokePath (p, PST (w * 3.5f, PST::curved, PST::rounded));
        g.setColour (c);
        g.strokePath (p, PST (w, PST::curved, PST::rounded));
    }

    inline void dotGlow (juce::Graphics& g, juce::Point<float> p, float r, juce::Colour c)
    {
        for (int k = 4; k >= 1; --k)
        {
            const float rr = r + (float) k * 3.f;
            g.setColour (c.withAlpha (0.05f));
            g.fillEllipse (p.x - rr, p.y - rr, rr * 2.f, rr * 2.f);
        }
        g.setColour (c);
        g.fillEllipse (p.x - r, p.y - r, r * 2.f, r * 2.f);
    }

    struct NeonLookAndFeel : juce::LookAndFeel_V4
    {
        NeonLookAndFeel()
        {
            setColour (juce::PopupMenu::backgroundColourId, panel);
            setColour (juce::PopupMenu::textColourId, text);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, purple.withAlpha (0.35f));
            setColour (juce::PopupMenu::highlightedTextColourId, white);
            setColour (juce::TextEditor::backgroundColourId, bg);
            setColour (juce::TextEditor::textColourId, white);
            setColour (juce::TextEditor::outlineColourId, purple.withAlpha (0.6f));
            setColour (juce::TextEditor::focusedOutlineColourId, purple);
            setColour (juce::CaretComponent::caretColourId, green);
            setColour (juce::ResizableWindow::backgroundColourId, bg);
        }
    };
}

// ---------------------------------------------------------------------------------------
// Minimal horizontal fader.
//  - drag: change value (shift = fine)    - double-click (left): type a number
//  - single right-click: reset to default - mouse wheel: nudge
class MiniFader : public juce::Component
{
public:
    explicit MiniFader (juce::String labelText) : label (std::move (labelText)) {}

    void setParameter (juce::RangedAudioParameter* p, juce::Colour c)
    {
        closeEditor();
        att.reset();
        param = p;
        colour = c;
        if (param != nullptr)
        {
            att = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); }, nullptr);
            att->sendInitialUpdate();
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (2.f, 0.f);

        g.setFont (Neon::font (10.f));
        g.setColour (Neon::dim);
        g.drawText (label, b.withHeight (14.f), juce::Justification::topLeft);

        if (param == nullptr)
            return;

        if (edit == nullptr)
        {
            g.setFont (Neon::font (11.f, true));
            g.setColour (Neon::text);
            g.drawText (param->getCurrentValueAsText(), b.withHeight (14.f), juce::Justification::topRight);
        }

        const float y = b.getBottom() - 9.f;
        const float w = b.getWidth();
        const float nv = param->getValue();
        const auto& r = param->getNormalisableRange();
        const bool bip = r.start < 0.f && r.end > 0.f;
        const float z = bip ? r.convertTo0to1 (0.f) : 0.f;

        g.setColour (Neon::line);
        g.fillRoundedRectangle (b.getX(), y - 1.f, w, 2.f, 1.f);

        const float x0 = b.getX() + std::min (z, nv) * w;
        const float x1 = b.getX() + std::max (z, nv) * w;
        g.setColour (colour.withAlpha (0.16f));
        g.fillRoundedRectangle (x0, y - 3.f, x1 - x0, 6.f, 3.f);
        g.setColour (colour);
        g.fillRoundedRectangle (x0, y - 1.f, x1 - x0, 2.f, 1.f);

        if (bip)
        {
            g.setColour (Neon::dim);
            g.fillRect (b.getX() + z * w - 0.5f, y - 4.f, 1.f, 8.f);
        }

        Neon::dotGlow (g, { b.getX() + nv * w, y }, 3.5f, colour);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (param == nullptr || ! att || ! isEnabled())
            return;

        if (e.mods.isPopupMenu())
        {
            att->setValueAsCompleteGesture (param->convertFrom0to1 (param->getDefaultValue()));
            return;
        }

        dragStart = param->getValue();
        att->beginGesture();
        gestureOn = true;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! gestureOn || param == nullptr)
            return;

        const float sens = e.mods.isShiftDown() ? 0.08f : 1.f;
        const float nv = juce::jlimit (0.f, 1.f, dragStart + sens * (float) e.getDistanceFromDragStartX() / (float) getWidth());
        att->setValueAsPartOfGesture (param->convertFrom0to1 (nv));
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (gestureOn && att)
            att->endGesture();
        gestureOn = false;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (param == nullptr || ! isEnabled() || e.mods.isPopupMenu())
            return;
        openEditor();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (param == nullptr || ! att || ! isEnabled())
            return;
        const float step = e.mods.isShiftDown() ? 0.004f : 0.03f;
        const float nv = juce::jlimit (0.f, 1.f, param->getValue() + w.deltaY * step * 10.f);
        att->setValueAsCompleteGesture (param->convertFrom0to1 (nv));
    }

    ~MiniFader() override { closeEditor(); }

private:
    void openEditor()
    {
        closeEditor();
        edit = std::make_unique<juce::TextEditor>();
        addAndMakeVisible (*edit);
        edit->setBounds (getLocalBounds().removeFromTop (17).removeFromRight (getWidth() / 2 + 10));
        edit->setFont (Neon::font (12.f));
        edit->setJustification (juce::Justification::centredRight);
        edit->setText (param->getCurrentValueAsText(), false);
        edit->selectAll();
        edit->onReturnKey = [this] { commit(); };
        edit->onEscapeKey = [this] { closeEditor(); };
        edit->onFocusLost = [this] { closeEditor(); };
        edit->grabKeyboardFocus();
        repaint();
    }

    void commit()
    {
        if (edit == nullptr || param == nullptr || ! att)
            return;
        const float norm = param->getValueForText (edit->getText());
        att->setValueAsCompleteGesture (param->convertFrom0to1 (juce::jlimit (0.f, 1.f, norm)));
        closeEditor();
    }

    void closeEditor()
    {
        if (edit == nullptr)
            return;
        auto* raw = edit.release();
        raw->setVisible (false);
        removeChildComponent (raw);
        juce::MessageManager::callAsync ([raw] { delete raw; });
        repaint();
    }

    juce::String label;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> att;
    std::unique_ptr<juce::TextEditor> edit;
    juce::Colour colour = Neon::white;
    float dragStart = 0.f;
    bool gestureOn = false;
};

// ---------------------------------------------------------------------------------------
// Segmented selector bound to a choice / bool parameter. Right-click = reset to default.
class SegmentedChoice : public juce::Component
{
public:
    explicit SegmentedChoice (juce::StringArray l) : labels (std::move (l)) {}

    void setParameter (juce::RangedAudioParameter* p, juce::Colour c)
    {
        att.reset();
        param = p;
        colour = c;
        if (param != nullptr)
        {
            att = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
                                                               { index = (int) (v + 0.5f); repaint(); }, nullptr);
            att->sendInitialUpdate();
        }
        repaint();
    }

    int getIndex() const { return index; }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const float w = b.getWidth() / (float) labels.size();

        g.setColour (Neon::line);
        g.drawRoundedRectangle (b.reduced (0.5f), 7.f, 1.f);

        for (int i = 0; i < labels.size(); ++i)
        {
            juce::Rectangle<float> seg (b.getX() + (float) i * w, b.getY(), w, b.getHeight());
            const bool sel = i == index;

            if (sel)
            {
                g.setColour (colour.withAlpha (0.16f));
                g.fillRoundedRectangle (seg.reduced (2.5f), 5.f);
                g.setColour (colour.withAlpha (0.7f));
                g.drawRoundedRectangle (seg.reduced (2.5f), 5.f, 1.f);
            }

            g.setFont (Neon::font (10.5f, sel));
            g.setColour (sel ? Neon::white : Neon::dim);
            g.drawText (labels[i], seg, juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (param == nullptr || ! att || ! isEnabled())
            return;

        if (e.mods.isPopupMenu())
        {
            att->setValueAsCompleteGesture (param->convertFrom0to1 (param->getDefaultValue()));
            return;
        }

        const int i = juce::jlimit (0, labels.size() - 1, (int) (e.position.x / ((float) getWidth() / (float) labels.size())));
        att->setValueAsCompleteGesture ((float) i);
    }

private:
    juce::StringArray labels;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> att;
    juce::Colour colour = Neon::white;
    int index = 0;
};

// ---------------------------------------------------------------------------------------
class PillButton : public juce::Component
{
public:
    std::function<void()> onClick;
    juce::String text;
    juce::Colour colour = Neon::white;
    bool selected = false, dimmed = false;

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (0.5f);
        const float a = dimmed ? 0.35f : 1.f;

        if (selected)
        {
            g.setColour (colour.withAlpha (0.16f * a));
            g.fillRoundedRectangle (b, 7.f);
            g.setColour (colour.withAlpha (0.8f * a));
        }
        else
            g.setColour (Neon::line);

        g.drawRoundedRectangle (b, 7.f, 1.f);
        g.setFont (Neon::font (11.f, selected));
        g.setColour ((selected ? colour : Neon::dim).withAlpha (a));
        g.drawText (text, getLocalBounds().reduced (6, 0), juce::Justification::centred, true);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (isEnabled() && contains (e.getPosition()) && ! e.mods.isPopupMenu() && onClick)
            onClick();
    }
};
