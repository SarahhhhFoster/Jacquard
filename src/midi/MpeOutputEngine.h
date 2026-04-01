#pragma once
#include "MpeAllocator.h"
#include "../model/ChordTimeline.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>

// MpeOutputEngine tracks which tones should be playing and generates
// MPE MIDI messages into a MidiBuffer each audio block.
//
// All methods that touch the MidiBuffer are called from the audio thread.
// The snapshot (list of active tones) is set from the message thread via
// setActiveTones(), which swaps an atomic pointer — no locks on the hot path.

struct ActiveNote
{
    juce::Uuid toneId;
    double     frequency = 0.0;
    int        slotIndex = -1;
    int        midiNote  = 0;
    bool       playing   = false;
};

class MpeOutputEngine
{
public:
    MpeOutputEngine();

    // Called from message thread when timeline or playhead changes.
    // Snapshot the set of tones that should be sounding right now.
    void setTargetTones(const std::vector<Tone>& tones);

    // Called from audio thread each block. Generates MPE messages.
    // sampleOffset = where in the buffer these events should land.
    void process(juce::MidiBuffer& midiOut,
                 int               sampleOffset,
                 uint64_t          blockStartFrame);

    // Called from audio thread when playback stops.
    void allNotesOff(juce::MidiBuffer& midiOut, int sampleOffset);

    // ── External MIDI device output (message thread) ──────────────────────────
    void setOutputDevice(std::unique_ptr<juce::MidiOutput> device);
    void sendSetupMessages(); // send MPE zone master + pitchbend range

    // Pitch bend calculation: ±48 semitone range
    static constexpr int kPitchBendSemitones = 48;
    static int pitchBendValue(double freq, int midiNote);

private:
    // Shared between message thread (writes) and audio thread (reads).
    // We use a simple pre-allocated array of active notes (max 15 = MPE channels).
    static constexpr int kMaxNotes = MpeAllocator::kNumChannels;

    std::array<ActiveNote, kMaxNotes> activeNotes_;
    MpeAllocator                      allocator_;
    uint64_t                          frameCounter_{ 0 };

    // Pending target tones — written from message thread, read by audio thread
    // on next process() call via a simple flag + vector swap.
    std::vector<Tone>  pendingTones_;
    std::atomic<bool>  pendingDirty_{ false };
    std::vector<Tone>  currentTargetTones_;

    std::unique_ptr<juce::MidiOutput> outputDevice_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MpeOutputEngine)
};
