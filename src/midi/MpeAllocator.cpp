#include "MpeAllocator.h"
#include <algorithm>
#include <limits>

MpeAllocator::MpeAllocator()
{
    for (int i = 0; i < kNumChannels; ++i)
    {
        slots_[i].channel = kFirstNoteChannel + i;
        slots_[i].active  = false;
        slots_[i].lastUsedFrame = 0;
    }
}

int MpeAllocator::allocate(int midiNote, uint64_t frame)
{
    // Find an inactive slot; prefer the one least recently used
    int     bestIdx      = -1;
    uint64_t oldestFrame = std::numeric_limits<uint64_t>::max();

    for (int i = 0; i < kNumChannels; ++i)
    {
        if (!slots_[i].active && slots_[i].lastUsedFrame < oldestFrame)
        {
            oldestFrame = slots_[i].lastUsedFrame;
            bestIdx     = i;
        }
    }
    if (bestIdx < 0) return -1; // all channels in use

    slots_[bestIdx].active       = true;
    slots_[bestIdx].midiNote     = midiNote;
    slots_[bestIdx].lastUsedFrame = frame;
    return bestIdx;
}

void MpeAllocator::release(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= kNumChannels) return;
    slots_[slotIndex].active   = false;
    slots_[slotIndex].midiNote = -1;
}

void MpeAllocator::releaseAll()
{
    for (auto& s : slots_)
    {
        s.active   = false;
        s.midiNote = -1;
    }
}
