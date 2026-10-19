#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

// parameter id helper: b1_freq, b2_gain, ...
inline juce::String bid (int band, const char* name)
{
    return "b" + juce::String (band + 1) + "_" + name;
}

struct Coeffs
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};

// type: 0 bell, 1 low shelf, 2 high shelf, 3 high-pass, 4 low-pass, 5 band-pass (detector)
inline Coeffs makeBiquad (int type, double fs, double f, double q, double gainDb)
{
    f = juce::jlimit (10.0, fs * 0.45, f);
    q = std::max (q, 0.05);

    const double w0 = 2.0 * juce::MathConstants<double>::pi * f / fs;
    const double cw = std::cos (w0), sw = std::sin (w0);
    const double alpha = sw / (2.0 * q);
    const double A = std::pow (10.0, gainDb / 40.0);

    double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;

    switch (type)
    {
        case 0: // bell
            b0 = 1 + alpha * A;  b1 = -2 * cw;  b2 = 1 - alpha * A;
            a0 = 1 + alpha / A;  a1 = -2 * cw;  a2 = 1 - alpha / A;
            break;
        case 1: // low shelf
        {
            const double s = 2.0 * std::sqrt (A) * alpha;
            b0 = A * ((A + 1) - (A - 1) * cw + s);
            b1 = 2 * A * ((A - 1) - (A + 1) * cw);
            b2 = A * ((A + 1) - (A - 1) * cw - s);
            a0 = (A + 1) + (A - 1) * cw + s;
            a1 = -2 * ((A - 1) + (A + 1) * cw);
            a2 = (A + 1) + (A - 1) * cw - s;
            break;
        }
        case 2: // high shelf
        {
            const double s = 2.0 * std::sqrt (A) * alpha;
            b0 = A * ((A + 1) + (A - 1) * cw + s);
            b1 = -2 * A * ((A - 1) + (A + 1) * cw);
            b2 = A * ((A + 1) + (A - 1) * cw - s);
            a0 = (A + 1) - (A - 1) * cw + s;
            a1 = 2 * ((A - 1) - (A + 1) * cw);
            a2 = (A + 1) - (A - 1) * cw - s;
            break;
        }
        case 3: // high-pass
            b0 = (1 + cw) / 2;  b1 = -(1 + cw);  b2 = (1 + cw) / 2;
            a0 = 1 + alpha;     a1 = -2 * cw;    a2 = 1 - alpha;
            break;
        case 4: // low-pass
            b0 = (1 - cw) / 2;  b1 = 1 - cw;     b2 = (1 - cw) / 2;
            a0 = 1 + alpha;     a1 = -2 * cw;    a2 = 1 - alpha;
            break;
        default: // band-pass, 0 dB peak
            b0 = alpha;  b1 = 0;  b2 = -alpha;
            a0 = 1 + alpha;  a1 = -2 * cw;  a2 = 1 - alpha;
            break;
    }

    Coeffs c;
    c.b0 = b0 / a0;  c.b1 = b1 / a0;  c.b2 = b2 / a0;
    c.a1 = a1 / a0;  c.a2 = a2 / a0;
    return c;
}

// Fills up to 4 cascaded stages. Types 3/4 are Butterworth cuts (slope 0=12, 1=24, 2=48 dB/oct).
// Returns number of stages.
inline int buildBandCoeffs (int type, int slope, double fs, double f, double q, double gainDb, Coeffs* out)
{
    if (type == 3 || type == 4)
    {
        static const double q1[1] = { 0.70710678 };
        static const double q2[2] = { 0.54119610, 1.30656296 };
        static const double q4[4] = { 0.50979558, 0.60134489, 0.89997622, 2.56291545 };

        const double* qs = q1;
        int n = 1;
        if (slope == 1) { qs = q2; n = 2; }
        else if (slope >= 2) { qs = q4; n = 4; }

        for (int i = 0; i < n; ++i)
            out[i] = makeBiquad (type, fs, f, qs[i], 0.0);
        return n;
    }

    out[0] = makeBiquad (type, fs, f, q, gainDb);
    return 1;
}

inline double magSq (const Coeffs& c, double w)
{
    const double cw = std::cos (w), c2 = std::cos (2 * w);
    const double sw = std::sin (w), s2 = std::sin (2 * w);
    const double nr = c.b0 + c.b1 * cw + c.b2 * c2;
    const double ni = -(c.b1 * sw + c.b2 * s2);
    const double dr = 1.0 + c.a1 * cw + c.a2 * c2;
    const double di = -(c.a1 * sw + c.a2 * s2);
    return (nr * nr + ni * ni) / (dr * dr + di * di + 1e-30);
}
