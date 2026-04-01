#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <optional>

// MPE lower zone: master = channel 1, note channels = 2..16 (15 slots).
// Allocates the least-recently-used free channel per note.

class MpeAllocator
{
public:
    static constexpr int kFirstNoteChannel = 2;
    static constexpr int kLastNoteChannel  = 16;
    static constexpr int kNumChannels      = kLastNoteChannel - kFirstNoteChannel + 1; // 15

    struct Slot
    {
        int  channel    = 0;    // MIDI channel (1-based)
        int  midiNote   = -1;   // nearest MIDI note number
        bool active     = false;
        uint64_t lastUsedFrame = 0;
    };

    MpeAllocator();

    // Allocate a channel for a new note. Returns the slot index or -1 if full.
    int allocate(int midiNote, uint64_t frame);

    // Release a slot by index.
    void release(int slotIndex);

    // Release all.
    void releaseAll();

    const Slot& slot(int idx) const { return slots_[idx]; }
    int numSlots() const { return kNumChannels; }

private:
    std::array<Slot, kNumChannels> slots_;
    uint64_t                       frameCounter_{ 0 };
};
