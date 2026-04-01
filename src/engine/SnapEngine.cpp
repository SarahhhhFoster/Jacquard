#include "SnapEngine.h"
#include <cmath>
#include <algorithm>
#include <limits>

// ── Consonance-curve snap ─────────────────────────────────────────────────────

double snapFrequency(double clickedFreq,
                     const std::vector<float>& curve,
                     double fMin,
                     double fMax,
                     const Options& opts)
{
    if (curve.empty()) return clickedFreq;

    int bins = (int)curve.size();

    // Convert clicked frequency to bin index
    double logMin  = std::log(fMin);
    double logMax  = std::log(fMax);
    double logFreq = std::log(std::max(clickedFreq, fMin));
    double t       = (logFreq - logMin) / (logMax - logMin);
    int    centerBin = (int)std::round(t * (bins - 1));
    centerBin = std::max(0, std::min(bins - 1, centerBin));

    // Convert snapRange (cents) to bins
    // Cents between two adjacent bins: 1200 * log2(fMax/fMin) / bins
    double centsPerBin = 1200.0 * std::log2(fMax / fMin) / bins;
    int toleranceBins  = (int)std::ceil(opts.snapRange / centsPerBin);
    int lo = std::max(0,       centerBin - toleranceBins);
    int hi = std::min(bins - 1, centerBin + toleranceBins);

    if (opts.snapStrategy == SnapStrategy::Slope)
    {
        // Find local maxima in search range, pick the highest
        int bestBin = -1;
        float bestVal = -1.0f;
        for (int i = lo + 1; i <= hi - 1; ++i)
        {
            if (curve[i] > curve[i - 1] && curve[i] > curve[i + 1])
            {
                if (curve[i] > bestVal)
                {
                    bestVal = curve[i];
                    bestBin = i;
                }
            }
        }
        if (bestBin >= 0)
        {
            // Parabolic interpolation for sub-bin accuracy
            double frac;
            if (bestBin > 0 && bestBin < bins - 1)
            {
                float y0 = curve[bestBin - 1];
                float y1 = curve[bestBin];
                float y2 = curve[bestBin + 1];
                float denom = 2.0f * (2.0f * y1 - y0 - y2);
                float offset = (denom > 1e-6f) ? (y0 - y2) / denom : 0.0f;
                frac = (bestBin + offset) / (bins - 1);
            }
            else
            {
                frac = (double)bestBin / (bins - 1);
            }
            return std::exp(logMin + frac * (logMax - logMin));
        }
    }
    else // Value strategy
    {
        // Pick highest consonance value in range, if above threshold
        int bestBin = -1;
        float bestVal = 0.0f;
        for (int i = lo; i <= hi; ++i)
        {
            if (curve[i] > bestVal)
            {
                bestVal = curve[i];
                bestBin = i;
            }
        }
        if (bestBin >= 0 && bestVal > 0.15f)  // threshold low enough to catch peaks
        {
            double frac = (double)bestBin / (bins - 1);
            return std::exp(logMin + frac * (logMax - logMin));
        }
    }

    return clickedFreq; // no snap target found
}

// ── Just-intonation snap ──────────────────────────────────────────────────────

static const struct { double ratio; const char* name; } kJIRatios[] = {
    { 1.0 / 1,  "Unison"           },
    { 16.0/ 15, "Minor 2nd (16:15)"},
    { 9.0 / 8,  "Major 2nd (9:8)"  },
    { 6.0 / 5,  "Minor 3rd (6:5)"  },
    { 5.0 / 4,  "Major 3rd (5:4)"  },
    { 4.0 / 3,  "Perfect 4th"      },
    { 7.0 / 5,  "Tritone (7:5)"    },
    { 3.0 / 2,  "Perfect 5th"      },
    { 8.0 / 5,  "Minor 6th (8:5)"  },
    { 5.0 / 3,  "Major 6th (5:3)"  },
    { 7.0 / 4,  "Harm. 7th (7:4)"  },
    { 9.0 / 5,  "Minor 7th (9:5)"  },
    { 15.0/ 8,  "Major 7th (15:8)" },
    { 2.0 / 1,  "Octave"           },
};

JIResult snapToJustIntonation(double clickedFreq, double referenceFreq)
{
    if (referenceFreq <= 0.0)
        referenceFreq = 261.63; // C4

    // Normalise clicked freq to within one octave above reference
    double ratio = clickedFreq / referenceFreq;
    while (ratio >= 2.0) ratio /= 2.0;
    while (ratio <  1.0) ratio *= 2.0;

    double bestDist = std::numeric_limits<double>::max();
    const char* bestName = "Unison";
    double bestRatio = 1.0;

    for (const auto& ji : kJIRatios)
    {
        double dist = std::abs(std::log2(ratio) - std::log2(ji.ratio));
        if (dist < bestDist)
        {
            bestDist  = dist;
            bestName  = ji.name;
            bestRatio = ji.ratio;
        }
    }

    // Scale bestRatio to match the octave of the clicked frequency
    double snapped = referenceFreq * bestRatio;
    while (snapped < clickedFreq / 1.5) snapped *= 2.0;
    while (snapped > clickedFreq * 1.5) snapped /= 2.0;

    return { snapped, bestName };
}

// ── Frequency → MIDI ──────────────────────────────────────────────────────────

MidiPitch freqToMidi(double freq)
{
    double midi = 69.0 + 12.0 * std::log2(freq / 440.0);
    int    note = (int)std::round(midi);
    double cents = (midi - note) * 100.0;
    return { note, cents };
}
