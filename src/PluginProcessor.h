#pragma once
#include "model/ChordTimeline.h"
#include "model/Options.h"
#include "engine/HeatmapData.h"
#include "ipc/SessionManager.h"
#include "midi/MpeOutputEngine.h"
#include "audio/SineBank.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <atomic>

class JacquardProcessor : public juce::AudioProcessor,
                          private juce::Timer
{
public:
    JacquardProcessor();
    ~JacquardProcessor() override;

    // ── AudioProcessor ────────────────────────────────────────────────────────
    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const AudioProcessor::BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool                        hasEditor() const override { return true; }

    const juce::String getName() const override { return "Jacquard"; }

    bool   acceptsMidi()  const override { return false; }
    bool   producesMidi() const override { return true;  }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& data) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ── Public accessors (message thread only) ────────────────────────────────
    ChordTimeline&   getTimeline()       { return timeline_; }
    Options&         getOptions()        { return options_;  }
    SessionManager&  getSessionManager() { return session_;  }
    HeatmapData&     getHeatmapData()    { return heatmap_;  }
    MpeOutputEngine& getMpeEngine()      { return mpe_;      }

    double getPlayheadPPQ() const { return playheadPPQ_.load(); }

    void applyOptions();
    void onTimelineChanged();
    void clearAll();

    // ── Preview (message thread) ──────────────────────────────────────────────
    // Outputs all enabled tones in `chord` via MPE (and sine bank in standalone)
    // for 800 ms, then silences.
    void triggerPreview(const Chord& chord);
    void stopPreview();

    // ── Standalone transport (message thread writes, audio thread reads) ──────
    void setStandalonePlaying(bool playing)  { standaloneIsPlaying_.store(playing); }
    void setStandalonePosition(double ppq)   { standalonePPQ_.store(ppq);           }
    void setStandaloneBpm(double bpm)        { standaloneBpm_.store(bpm);           }
    double getStandaloneBpm() const          { return standaloneBpm_.load();        }

private:
    void timerCallback() override;
    std::vector<Tone> tonesAtPPQ(double ppq) const;

    ChordTimeline   timeline_;
    Options         options_;
    SessionManager  session_{ timeline_ };
    HeatmapData     heatmap_;
    MpeOutputEngine mpe_;
    SineBank        sineBank_;

    // Audio thread state
    double          sampleRate_{ 44100.0 };
    uint64_t        blockFrame_{ 0 };
    bool            wasPlaying_{ false };

    // Playhead shared between audio thread and UI (written on both threads)
    std::atomic<double> playheadPPQ_{ 0.0 };

    // Standalone internal transport
    std::atomic<bool>   standaloneIsPlaying_{ false };
    std::atomic<double> standalonePPQ_{ 0.0 };
    std::atomic<double> standaloneBpm_{ 120.0 };

    // Preview state (message thread only)
    bool         previewActive_{ false };
    uint32_t     previewStartMs_{ 0 };
    static constexpr int kPreviewDurationMs = 800;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JacquardProcessor)
};
