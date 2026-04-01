#pragma once
#include "SetharesEngine.h"
#include "../model/Options.h"

// Given a consonance curve and a clicked frequency, find the best snap target.
// Returns the snapped frequency in Hz.

double snapFrequency(double clickedFreq,
                     const std::vector<float>& curve,
                     double fMin,
                     double fMax,
                     const Options& opts);

// Just-intonation snap: find the nearest JI ratio multiple of a reference
// frequency.  If referenceFreq <= 0, uses C4 (261.63 Hz) octave-reduced.
// Returns the snapped frequency and a human-readable interval name.
struct JIResult
{
    double      frequency;
    const char* name;
};

JIResult snapToJustIntonation(double clickedFreq, double referenceFreq = 0.0);

// Convert Hz to the nearest MIDI note number and cents deviation.
struct MidiPitch { int note; double cents; };
MidiPitch freqToMidi(double freq);
