#pragma once
#include <vector>

// ── Sethares Dissonance / Consonance Engine ───────────────────────────────────
//
// Based on: Sethares (1993), "Local Consonance and the Relationship between
// Timbre and Scale."  Journal of the Acoustical Society of America 94(3).
//
// The core model computes the roughness between pairs of sinusoidal partials
// and sums them to yield a total dissonance.  We invert this to get consonance.

struct Partial
{
    double frequency;  // Hz
    double amplitude;  // linear, > 0
};

// Generate a harmonic series for a fundamental at `freq`.
// Amplitudes follow a 1/n decay (n = harmonic number).
std::vector<Partial> makeHarmonics(double freq, int numPartials = 6, double baseAmplitude = 1.0);

// Roughness between two pure partials (f1 ≤ f2 enforced internally).
// Returns a non-negative value; 0 when partials are identical or far apart.
double pairRoughness(double f1, double a1, double f2, double a2);

// Total dissonance of a set of partials (sum over all pairs).
double totalDissonance(const std::vector<Partial>& partials);

// Total dissonance of a probe frequency (with harmonics) against a set of
// existing partials.  Lower = more consonant.
double probeDissonance(double probeFreq,
                       const std::vector<Partial>& context,
                       int numProbePartials = 6);

// Generate a consonance curve: numBins values in [0, 1] for frequencies
// uniformly spaced on a log scale from fMin to fMax.
// contextFreqs is the set of existing tone fundamentals (their harmonics are
// generated internally).
// If contextFreqs is empty, returns a uniform 1.0 curve (no constraint).
std::vector<float> buildConsonanceCurve(const std::vector<double>& contextFreqs,
                                        double fMin,
                                        double fMax,
                                        int numBins,
                                        int numHarmonics = 6);

// Map a pixel row (0 = top = high freq) to frequency (Hz) on a log scale.
double rowToFreq(int row, int totalRows, double fMin, double fMax);

// Map a frequency to the nearest pixel row.
int freqToRow(double freq, int totalRows, double fMin, double fMax);
