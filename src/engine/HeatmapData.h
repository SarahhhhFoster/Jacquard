#pragma once
#include "SetharesEngine.h"
#include "../model/ChordTimeline.h"
#include "../model/Options.h"
#include <juce_core/juce_core.h>
#include <memory>
#include <atomic>

// HeatmapData holds the consonance curves for all chords in the timeline.
// Computing them is expensive so they are cached per chord and rebuilt
// on any timeline change from a background thread.
//
// Once built, data is read-only and shared via shared_ptr so the UI can
// read it without any lock while a new build is in progress.

struct ChordHeatmap
{
    juce::Uuid chordId;
    std::vector<float> curveCurrent;  // consonance from this chord's own tones
    std::vector<float> curvePrev;     // consonance from prev chord's tones (empty if none)
    std::vector<float> curveNext;     // consonance from next chord's tones (empty if none)
    std::vector<bool>  islandMask;    // true = within snapRange of a local max in curveCurrent
    int numBins = 0;
};

class HeatmapData : public juce::AsyncUpdater
{
public:
    HeatmapData() = default;
    ~HeatmapData() override = default;

    // Call from the message thread whenever the timeline or options change.
    // Schedules a background recompute.
    void invalidate(const ChordTimeline& timeline, const Options& opts);

    // Called from the message thread when a new build is ready.
    // Callback receives a shared_ptr to the new data.
    std::function<void(std::shared_ptr<std::vector<ChordHeatmap>>)> onRebuilt;

    // Get the currently published (possibly stale) data.
    std::shared_ptr<const std::vector<ChordHeatmap>> getCurrent() const
    {
        return std::atomic_load(&current_);
    }

private:
    void handleAsyncUpdate() override;

    // Snapshotted inputs for the background thread
    std::vector<Chord>  pendingChords_;
    Options             pendingOptions_;
    bool                dirty_{ false };

    std::shared_ptr<const std::vector<ChordHeatmap>> current_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeatmapData)
};
