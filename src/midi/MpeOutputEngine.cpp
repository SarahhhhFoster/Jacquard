#include "MpeOutputEngine.h"
#include "../engine/SnapEngine.h"
#include <cmath>

MpeOutputEngine::MpeOutputEngine()
{
    for (auto& n : activeNotes_)
        n = {};
}

// ── Message-thread API ────────────────────────────────────────────────────────

void MpeOutputEngine::setTargetTones(const std::vector<Tone>& tones)
{
    // Called on message thread.  Store pending and flag dirty.
    // The audio thread will pick up on next process().
    pendingTones_  = tones;
    pendingDirty_.store(true, std::memory_order_release);
}

void MpeOutputEngine::setOutputDevice(std::unique_ptr<juce::MidiOutput> device)
{
    outputDevice_ = std::move(device);
    if (outputDevice_) sendSetupMessages();
}

void MpeOutputEngine::sendSetupMessages()
{
    if (!outputDevice_) return;

    // MPE Lower Zone setup:
    // On channel 1 (master), set RPN 6 (zone size) to 15 (channels 2-16)
    // Then set pitchbend range (RPN 0) to kPitchBendSemitones on each note channel.

    // RPN 6 = zone master config; value MSB = number of note channels
    // Per MPE spec: CC 100=6, 101=0, 6=numChannels, 38=0 on master channel
    auto sendRPN = [this](int ch, int rpnMSB, int rpnLSB, int valMSB, int valLSB)
    {
        auto sendCC = [this](int ch, int cc, int val)
        {
            outputDevice_->sendMessageNow(juce::MidiMessage::controllerEvent(ch, cc, val));
        };
        sendCC(ch, 101, rpnMSB);
        sendCC(ch, 100, rpnLSB);
        sendCC(ch,   6, valMSB);
        sendCC(ch,  38, valLSB);
        // Reset RPN to null (prevents accidental changes)
        sendCC(ch, 101, 127);
        sendCC(ch, 100, 127);
    };

    // Zone master (ch 1): RPN 6 = 15 (note channels count)
    sendRPN(1, 0, 6, MpeAllocator::kNumChannels, 0);

    // Each note channel: RPN 0 = pitch bend range = kPitchBendSemitones
    for (int ch = MpeAllocator::kFirstNoteChannel;
         ch <= MpeAllocator::kLastNoteChannel; ++ch)
    {
        sendRPN(ch, 0, 0, kPitchBendSemitones, 0);
    }
}

// ── Audio-thread processing ───────────────────────────────────────────────────

int MpeOutputEngine::pitchBendValue(double freq, int midiNote)
{
    double midiFreq  = 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
    double semitones = 12.0  * std::log2(freq / midiFreq);
    double normalised = semitones / kPitchBendSemitones;
    int value = (int)std::round(normalised * 8191.0) + 8192;
    return juce::jlimit(0, 16383, value);
}

void MpeOutputEngine::process(juce::MidiBuffer& midiOut,
                               int               sampleOffset,
                               uint64_t          blockStartFrame)
{
    frameCounter_ = blockStartFrame;

    // Consume pending target tones (written on message thread)
    if (pendingDirty_.load(std::memory_order_acquire))
    {
        currentTargetTones_ = pendingTones_;
        pendingDirty_.store(false, std::memory_order_release);
    }

    // Determine which tones to start and which to stop.
    // "Target" = tones that should be sounding now.
    // "Active" = tones currently allocated an MPE channel.

    // Mark all active notes as "needs check"
    std::array<bool, kMaxNotes> stillNeeded{};

    for (auto& target : currentTargetTones_)
    {
        bool found = false;
        for (int i = 0; i < kMaxNotes; ++i)
        {
            if (activeNotes_[i].playing && activeNotes_[i].toneId == target.id)
            {
                stillNeeded[i] = true;
                found = true;
                break;
            }
        }

        if (!found)
        {
            // Start a new note
            auto mp   = freqToMidi(target.frequency);
            int  slot = allocator_.allocate(mp.note, blockStartFrame);
            if (slot < 0) continue; // no channels left

            int  ch  = allocator_.slot(slot).channel;
            int  pb  = pitchBendValue(target.frequency, mp.note);
            int  vel = 100;

            // pitchbend before note-on (MPE spec)
            midiOut.addEvent(juce::MidiMessage::pitchWheel(ch, pb - 8192), sampleOffset);
            midiOut.addEvent(juce::MidiMessage::noteOn(ch, mp.note, (uint8_t)vel), sampleOffset);

            // Also send to external device if open
            if (outputDevice_)
            {
                outputDevice_->sendMessageNow(juce::MidiMessage::pitchWheel(ch, pb - 8192));
                outputDevice_->sendMessageNow(juce::MidiMessage::noteOn(ch, mp.note, (uint8_t)vel));
            }

            activeNotes_[slot] = { target.id, target.frequency, slot, mp.note, true };
            stillNeeded[slot]  = true;
        }
    }

    // Stop notes no longer needed
    for (int i = 0; i < kMaxNotes; ++i)
    {
        if (activeNotes_[i].playing && !stillNeeded[i])
        {
            int ch = allocator_.slot(i).channel;
            midiOut.addEvent(juce::MidiMessage::noteOff(ch, activeNotes_[i].midiNote), sampleOffset);
            if (outputDevice_)
                outputDevice_->sendMessageNow(juce::MidiMessage::noteOff(ch, activeNotes_[i].midiNote));

            allocator_.release(i);
            activeNotes_[i].playing = false;
        }
    }
}

void MpeOutputEngine::allNotesOff(juce::MidiBuffer& midiOut, int sampleOffset)
{
    for (int i = 0; i < kMaxNotes; ++i)
    {
        if (activeNotes_[i].playing)
        {
            int ch = allocator_.slot(i).channel;
            midiOut.addEvent(juce::MidiMessage::noteOff(ch, activeNotes_[i].midiNote), sampleOffset);
            if (outputDevice_)
                outputDevice_->sendMessageNow(juce::MidiMessage::noteOff(ch, activeNotes_[i].midiNote));
            activeNotes_[i].playing = false;
        }
    }
    allocator_.releaseAll();
    currentTargetTones_.clear();
}
