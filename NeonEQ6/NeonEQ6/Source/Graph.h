#pragma once
#include "Ui.h"
#include "PluginProcessor.h"

class GraphView : public juce::Component,
                  private juce::Timer
{
public:
    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;

    explicit GraphView (NeonEQProcessor& p)
        : proc (p),
          fft (fftOrder),
          window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false)
    {
        smooth.fill (-120.f);
        fftData.fill (0.f);
        startTimerHz (30);
    }

    std::function<void (int)> onSelect;

    void setSelected (int i) { selected = i; repaint(); }

    // ------------------------------------------------------------ geometry
    juce::Rectangle<float> plot() const
    {
        return getLocalBounds().toFloat().reduced (34.f, 10.f).withTrimmedBottom (14.f);
    }
    float freqToX (double f) const
    {
        auto a = plot();
        return a.getX() + a.getWidth() * (float) (std::log10 (f / 20.0) / 3.0);
    }
    float xToFreq (float x) const
    {
        auto a = plot();
        return 20.f * std::pow (1000.f, (x - a.getX()) / a.getWidth());
    }
    float dbToY (double db) const
    {
        auto a = plot();
        return a.getCentreY() - (float) db / 18.f * a.getHeight() * 0.5f;
    }
    float yToDb (float y) const
    {
        auto a = plot();
        return (a.getCentreY() - y) / (a.getHeight() * 0.5f) * 18.f;
    }
    float specToY (float db) const
    {
        auto a = plot();
        return a.getY() + juce::jlimit (0.f, 1.f, -db / 90.f) * a.getHeight();
    }

    // ------------------------------------------------------------ band data
    struct BandView
    {
        bool on = true, dynActive = false;
        int type = 0, slope = 0;
        double f = 1000, q = 1, g = 0, dyn = 0;
    };

    float rv (int b, const char* n) const { return proc.apvts.getRawParameterValue (bid (b, n))->load(); }
    juce::RangedAudioParameter* par (int b, const char* n) { return proc.apvts.getParameter (bid (b, n)); }
    static bool hasGain (int type) { return type <= 2; }

    BandView readBand (int b) const
    {
        BandView v;
        v.on = rv (b, "on") > 0.5f;
        v.type = (int) (rv (b, "type") + 0.5f);
        v.slope = (int) (rv (b, "slope") + 0.5f);
        v.f = rv (b, "freq");
        v.q = rv (b, "q");
        v.g = rv (b, "gain");
        v.dynActive = v.on && rv (b, "dyn") > 0.5f && v.type <= 2;
        v.dyn = v.dynActive ? (double) proc.dynDb[(size_t) b].load() : 0.0;
        return v;
    }

    juce::Point<float> nodePos (const BandView& v, bool withDyn) const
    {
        const double db = hasGain (v.type) ? v.g + (withDyn ? v.dyn : 0.0) : 0.0;
        return { freqToX (v.f), dbToY (db) };
    }

    int hitTest (juce::Point<float> p) const
    {
        int best = -1;
        float bestD = 15.f;
        for (int b = 0; b < 6; ++b)
        {
            const auto np = nodePos (readBand (b), true);
            const float d = np.getDistanceFrom (p);
            if (d < bestD) { bestD = d; best = b; }
        }
        return best;
    }

    // ------------------------------------------------------------ painting
    void paint (juce::Graphics& g) override
    {
        auto full = getLocalBounds().toFloat();
        auto a = plot();
        sr = proc.sampleRateAtomic.load();
        if (sr < 8000.0) sr = 44100.0;

        g.setColour (Neon::panel);
        g.fillRoundedRectangle (full, 10.f);
        g.setColour (Neon::line);
        g.drawRoundedRectangle (full.reduced (0.5f), 10.f, 1.f);

        // grid
        g.setFont (Neon::font (9.5f));
        const double fl[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
        for (double f : fl)
        {
            const float x = freqToX (f);
            const bool major = f == 100 || f == 1000 || f == 10000;
            g.setColour (Neon::line.withAlpha (major ? 0.9f : 0.45f));
            g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
            if (major || f == 20 || f == 20000)
            {
                g.setColour (Neon::dim);
                const juce::String t = f >= 1000 ? juce::String (f / 1000.0, 0) + "k" : juce::String ((int) f);
                g.drawText (t, juce::Rectangle<float> (x - 18.f, a.getBottom() + 1.f, 36.f, 13.f), juce::Justification::centred);
            }
        }
        for (int db = -12; db <= 12; db += 6)
        {
            const float y = dbToY (db);
            g.setColour (Neon::line.withAlpha (db == 0 ? 1.f : 0.5f));
            g.drawHorizontalLine ((int) y, a.getX(), a.getRight());
            g.setColour (Neon::dim);
            g.drawText ((db > 0 ? "+" : "") + juce::String (db), juce::Rectangle<float> (2.f, y - 7.f, 28.f, 14.f),
                        juce::Justification::centredRight);
        }

        g.saveState();
        g.reduceClipRegion (a.toNearestInt().expanded (1));

        // spectrum
        if (proc.apvts.getRawParameterValue ("an_on")->load() > 0.5f)
        {
            juce::Path line, fill;
            const int step = 2;
            bool first = true;
            for (float x = a.getX(); x <= a.getRight() + 0.5f; x += (float) step)
            {
                const float v = specAt (xToFreq (x - 1.f), xToFreq (x + 1.f));
                const float y = specToY (v);
                if (first) { line.startNewSubPath (x, y); fill.startNewSubPath (x, a.getBottom()); fill.lineTo (x, y); first = false; }
                else { line.lineTo (x, y); fill.lineTo (x, y); }
            }
            fill.lineTo (a.getRight(), a.getBottom());
            fill.closeSubPath();

            g.setGradientFill (juce::ColourGradient (Neon::purple.withAlpha (0.32f), 0.f, a.getY(),
                                                     Neon::purple.withAlpha (0.0f), 0.f, a.getBottom(), false));
            g.fillPath (fill);
            Neon::strokeGlow (g, line, Neon::pink.withAlpha (0.75f), 1.0f);
        }

        // eq curve
        std::array<std::array<Coeffs, 4>, 6> cf {};
        std::array<int, 6> nst {};
        for (int b = 0; b < 6; ++b)
        {
            auto v = readBand (b);
            nst[(size_t) b] = v.on ? buildBandCoeffs (v.type, v.slope, sr, v.f, v.q, v.g + v.dyn, cf[(size_t) b].data()) : 0;
        }

        auto dbAt = [&] (double f, int only)
        {
            const double w = 2.0 * juce::MathConstants<double>::pi * f / sr;
            double db = 0.0;
            for (int b = 0; b < 6; ++b)
            {
                if (only >= 0 && b != only) continue;
                for (int s = 0; s < nst[(size_t) b]; ++s)
                    db += 10.0 * std::log10 (magSq (cf[(size_t) b][(size_t) s], w) + 1e-20);
            }
            return db;
        };

        const float yZero = dbToY (0.0);
        {   // selected band footprint
            juce::Path sel;
            sel.startNewSubPath (a.getX(), yZero);
            for (float x = a.getX(); x <= a.getRight() + 0.5f; x += 2.f)
                sel.lineTo (x, juce::jlimit (a.getY(), a.getBottom(), dbToY (dbAt (xToFreq (x), selected))));
            sel.lineTo (a.getRight(), yZero);
            sel.closeSubPath();
            g.setColour (Neon::bandColour (selected).withAlpha (0.12f));
            g.fillPath (sel);
        }

        juce::Path curve;
        bool first = true;
        for (float x = a.getX(); x <= a.getRight() + 0.5f; x += 2.f)
        {
            const float y = juce::jlimit (a.getY(), a.getBottom(), dbToY (dbAt (xToFreq (x), -1)));
            if (first) { curve.startNewSubPath (x, y); first = false; }
            else curve.lineTo (x, y);
        }
        Neon::strokeGlow (g, curve, Neon::white, 1.6f);

        g.restoreState();

        // nodes
        for (int b = 0; b < 6; ++b)
        {
            const auto v = readBand (b);
            const auto c = Neon::bandColour (b);
            const auto p = nodePos (v, true);

            if (! v.on)
            {
                g.setColour (Neon::dim.withAlpha (0.5f));
                g.drawEllipse (p.x - 6.f, p.y - 6.f, 12.f, 12.f, 1.f);
                continue;
            }

            if (v.dynActive && std::abs (v.dyn) > 0.05)
            {
                const auto ghost = nodePos (v, false);
                g.setColour (c.withAlpha (0.5f));
                g.drawEllipse (ghost.x - 5.f, ghost.y - 5.f, 10.f, 10.f, 1.f);
                g.drawLine (ghost.x, ghost.y, p.x, p.y, 1.f);
            }

            const bool sel = b == selected;
            if (sel)
            {
                g.setColour (c.withAlpha (0.9f));
                g.drawEllipse (p.x - 12.f, p.y - 12.f, 24.f, 24.f, 1.2f);
            }
            Neon::dotGlow (g, p, sel ? 8.f : 6.5f, c);
            g.setFont (Neon::font (9.f, true));
            g.setColour (Neon::bg);
            g.drawText (juce::String (b + 1), juce::Rectangle<float> (p.x - 8.f, p.y - 8.f, 16.f, 16.f).toNearestInt(),
                        juce::Justification::centred);
        }
    }

    // ------------------------------------------------------------ mouse
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int h = hitTest (e.position);

        if (e.mods.isPopupMenu())
        {
            if (h >= 0)
            {
                select (h);
                for (const char* n : { "freq", "gain", "q" })
                {
                    auto* p = par (h, n);
                    p->beginChangeGesture();
                    p->setValueNotifyingHost (p->getDefaultValue());
                    p->endChangeGesture();
                }
            }
            return;
        }

        dragBand = h;
        if (h < 0)
            return;

        select (h);
        gestF = par (h, "freq");
        gestG = par (h, "gain");
        gestF->beginChangeGesture();
        gestG->beginChangeGesture();

        if (auto* on = par (h, "on"); on->getValue() < 0.5f)
        {
            on->beginChangeGesture();
            on->setValueNotifyingHost (1.f);
            on->endChangeGesture();
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragBand < 0 || gestF == nullptr)
            return;

        setReal (gestF, juce::jlimit (20.f, 20000.f, xToFreq (e.position.x)));
        if (hasGain ((int) (rv (dragBand, "type") + 0.5f)))
            setReal (gestG, juce::jlimit (-18.f, 18.f, yToDb (e.position.y)));
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragBand >= 0)
        {
            gestF->endChangeGesture();
            gestG->endChangeGesture();
        }
        dragBand = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            return;

        const int h = hitTest (e.position);
        if (h >= 0)
        {
            auto* on = par (h, "on");
            on->beginChangeGesture();
            on->setValueNotifyingHost (on->getValue() > 0.5f ? 0.f : 1.f);
            on->endChangeGesture();
            return;
        }

        for (int b = 0; b < 6; ++b)
            if (rv (b, "on") < 0.5f)
            {
                auto* on = par (b, "on");
                on->beginChangeGesture();
                on->setValueNotifyingHost (1.f);
                on->endChangeGesture();
                gesture (par (b, "freq"), juce::jlimit (20.f, 20000.f, xToFreq (e.position.x)));
                if (hasGain ((int) (rv (b, "type") + 0.5f)))
                    gesture (par (b, "gain"), juce::jlimit (-18.f, 18.f, yToDb (e.position.y)));
                select (b);
                return;
            }
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int h = hitTest (e.position);
        if (h < 0)
            return;
        gesture (par (h, "q"), rv (h, "q") * std::pow (1.15f, w.deltaY * 8.f));
    }

private:
    static void setReal (juce::RangedAudioParameter* p, float v) { p->setValueNotifyingHost (p->convertTo0to1 (v)); }

    static void gesture (juce::RangedAudioParameter* p, float v)
    {
        p->beginChangeGesture();
        setReal (p, v);
        p->endChangeGesture();
    }

    void select (int b)
    {
        selected = b;
        if (onSelect) onSelect (b);
        repaint();
    }

    float specAt (float f0, float f1) const
    {
        constexpr int nb = fftSize / 2;
        const float binHz = (float) sr / (float) fftSize;
        const float p0 = f0 / binHz, p1 = f1 / binHz;

        if (p1 - p0 < 1.f)
        {
            const float pc = 0.5f * (p0 + p1);
            const int i = juce::jlimit (0, nb - 2, (int) pc);
            const float fr = juce::jlimit (0.f, 1.f, pc - (float) i);
            return smooth[(size_t) i] * (1.f - fr) + smooth[(size_t) i + 1] * fr;
        }

        float m = -200.f;
        const int k0 = juce::jlimit (0, nb - 1, (int) p0);
        const int k1 = juce::jlimit (0, nb - 1, (int) std::ceil (p1));
        for (int k = k0; k <= k1; ++k)
            m = std::max (m, smooth[(size_t) k]);
        return m;
    }

    void timerCallback() override
    {
        if (proc.apvts.getRawParameterValue ("an_on")->load() > 0.5f
            && proc.scope.readLatest (fftData.data(), fftSize))
        {
            std::fill (fftData.begin() + fftSize, fftData.end(), 0.f);
            window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
            fft.performFrequencyOnlyForwardTransform (fftData.data());

            const int sp = juce::jlimit (0, 2, (int) (proc.apvts.getRawParameterValue ("an_speed")->load() + 0.5f));
            static const float up[3] = { 0.85f, 0.55f, 0.22f };
            static const float dn[3] = { 0.45f, 0.18f, 0.05f };

            for (int k = 0; k < fftSize / 2; ++k)
            {
                // Hann window, one-sided: a full-scale sine reads 0 dB. No tilt applied.
                const float db = juce::Decibels::gainToDecibels (fftData[(size_t) k] / ((float) fftSize * 0.25f), -120.f);
                auto& s = smooth[(size_t) k];
                s += (db - s) * (db > s ? up[sp] : dn[sp]);
            }
        }
        repaint();
    }

    NeonEQProcessor& proc;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, fftSize * 2> fftData;
    std::array<float, fftSize / 2> smooth;
    double sr = 44100.0;
    int selected = 0;
    int dragBand = -1;
    juce::RangedAudioParameter *gestF = nullptr, *gestG = nullptr;
};
