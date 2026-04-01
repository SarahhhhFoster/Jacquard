#include "SetharesEngine.h"
#include <cmath>
#include <algorithm>

// ── Sethares constants ────────────────────────────────────────────────────────
// From the original 1993 paper: fitted to psychoacoustic roughness data.
static constexpr double kDStar = 0.24;
static constexpr double kS1    = 0.021;
static constexpr double kS2    = 19.0;
static constexpr double kB1    = 3.51;
static constexpr double kB2    = 5.75;

// ── Harmonic series ───────────────────────────────────────────────────────────

std::vector<Partial> makeHarmonics(double freq, int numPartials, double baseAmplitude)
{
    std::vector<Partial> result;
    result.reserve(numPartials);
    for (int k = 1; k <= numPartials; ++k)
        result.push_back({ freq * k, baseAmplitude / k });
    return result;
}

// ── Core roughness function ───────────────────────────────────────────────────

double pairRoughness(double f1, double a1, double f2, double a2)
{
    if (f1 > f2) { std::swap(f1, f2); std::swap(a1, a2); }

    double df = f2 - f1;
    if (df < 1e-9) return 0.0;  // identical frequencies don't beat

    double s = kDStar / (kS1 * f1 + kS2);
    double y = s * df;

    // Both terms are positive; b2 > b1 so result is non-negative.
    return a1 * a2 * (std::exp(-kB1 * y) - std::exp(-kB2 * y));
}

// ── Total dissonance of a partial set ────────────────────────────────────────

double totalDissonance(const std::vector<Partial>& partials)
{
    double roughness = 0.0;
    for (size_t i = 0; i < partials.size(); ++i)
        for (size_t j = i + 1; j < partials.size(); ++j)
            roughness += pairRoughness(partials[i].frequency, partials[i].amplitude,
                                       partials[j].frequency, partials[j].amplitude);
    return roughness;
}

// ── Probe dissonance ──────────────────────────────────────────────────────────

double probeDissonance(double probeFreq,
                       const std::vector<Partial>& context,
                       int numProbePartials)
{
    if (context.empty()) return 0.0;

    auto probePartials = makeHarmonics(probeFreq, numProbePartials, 1.0);
    double roughness = 0.0;

    for (const auto& pp : probePartials)
        for (const auto& cp : context)
            roughness += pairRoughness(pp.frequency, pp.amplitude,
                                       cp.frequency, cp.amplitude);
    return roughness;
}

// ── Consonance curve ──────────────────────────────────────────────────────────

std::vector<float> buildConsonanceCurve(const std::vector<double>& contextFreqs,
                                        double fMin,
                                        double fMax,
                                        int numBins,
                                        int numHarmonics)
{
    std::vector<float> curve(numBins, 1.0f);
    if (contextFreqs.empty()) return curve;

    // Build context partial set from all context frequencies
    std::vector<Partial> context;
    context.reserve(contextFreqs.size() * numHarmonics);
    for (double f : contextFreqs)
    {
        auto h = makeHarmonics(f, numHarmonics, 1.0);
        context.insert(context.end(), h.begin(), h.end());
    }

    double logMin = std::log(fMin);
    double logMax = std::log(fMax);

    // Compute raw roughness for each bin
    double maxRoughness = 0.0;
    std::vector<double> rawRoughness(numBins);
    for (int i = 0; i < numBins; ++i)
    {
        double t = (double)i / (numBins - 1);
        double freq = std::exp(logMin + t * (logMax - logMin));
        double r = probeDissonance(freq, context, numHarmonics);
        rawRoughness[i] = r;
        if (r > maxRoughness) maxRoughness = r;
    }

    // Normalise to [0, 1]: consonance = 1 - (roughness / max)
    if (maxRoughness > 1e-12)
        for (int i = 0; i < numBins; ++i)
            curve[i] = 1.0f - (float)(rawRoughness[i] / maxRoughness);

    return curve;
}

// ── Frequency ↔ row mapping ───────────────────────────────────────────────────

double rowToFreq(int row, int totalRows, double fMin, double fMax)
{
    // Row 0 = top = fMax; row totalRows-1 = bottom = fMin
    double t = (double)row / (totalRows - 1);
    return std::exp(std::log(fMax) + t * (std::log(fMin) - std::log(fMax)));
}

int freqToRow(double freq, int totalRows, double fMin, double fMax)
{
    if (freq <= fMin) return totalRows - 1;
    if (freq >= fMax) return 0;
    double t = (std::log(fMax) - std::log(freq)) / (std::log(fMax) - std::log(fMin));
    int row = (int)std::round(t * (totalRows - 1));
    return std::max(0, std::min(totalRows - 1, row));
}
