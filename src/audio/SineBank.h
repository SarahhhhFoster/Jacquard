#pragma once
#include "../model/Tone.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>
#include <atomic>
#include <cmath>

// SineBank synthesises simple sine wave tones for standalone audio preview.
// setTones() is called from the message thread; process() from the audio thread.
// Synchronisation uses an atomic dirty flag + pending-copy swap.

class SineBank
{
public:
    static constexpr int kMaxOscillators = 15;

    void setSampleRate(double sr) noexcept { sampleRate_ = sr; }

    // Message thread: update target tones
    void setTones(const std::vector<Tone>& tones)
    {
        pending_ = tones;
        dirty_.store(true, std::memory_order_release);
    }

    void clear()
    {
        pending_.clear();
        dirty_.store(true, std::memory_order_release);
    }

    // Audio thread: mix oscillators into buffer
    void process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
    {
        if (dirty_.load(std::memory_order_acquire))
        {
            applyPending();
            dirty_.store(false, std::memory_order_release);
        }

        int numActive = 0;
        for (auto& o : oscillators_)
            if (o.active) ++numActive;

        if (numActive == 0) return;

        // Normalise amplitude to avoid clipping
        const float amp = 0.25f / (float)numActive;
        const double twoPi = juce::MathConstants<double>::twoPi;

        for (auto& o : oscillators_)
        {
            if (!o.active) continue;
            double phaseInc = twoPi * o.freqHz / sampleRate_;
            for (int c = 0; c < buffer.getNumChannels(); ++c)
            {
                auto* data = buffer.getWritePointer(c, startSample);
                double phase = o.phase;
                for (int i = 0; i < numSamples; ++i)
                {
                    data[i] += (float)(amp * std::sin(phase));
                    phase += phaseInc;
                }
            }
            o.phase += phaseInc * numSamples;
            // Keep phase in [0, 2π) to prevent precision loss
            o.phase = std::fmod(o.phase, twoPi);
        }
    }

private:
    struct Oscillator { double freqHz = 0.0; double phase = 0.0; bool active = false; };

    void applyPending()
    {
        for (auto& o : oscillators_) o.active = false;

        int slot = 0;
        for (const auto& t : pending_)
        {
            if (slot >= kMaxOscillators) break;
            if (!t.enabled || t.frequency <= 0.0) continue;
            oscillators_[slot].freqHz = t.frequency;
            oscillators_[slot].active = true;
            ++slot;
        }
    }

    std::array<Oscillator, kMaxOscillators> oscillators_{};
    double sampleRate_{ 44100.0 };
    std::vector<Tone> pending_;
    std::atomic<bool> dirty_{ false };
};
