#pragma once
#include "Chord.h"
#include <juce_events/juce_events.h>
#include <shared_mutex>
#include <functional>
#include <optional>

// Thread-safe chord timeline.
// Write access uses a unique_lock; read access uses a shared_lock.
// The audio thread must NOT lock this directly — it reads from a snapshot
// maintained by PluginProcessor.
class ChordTimeline
{
public:
    // ── Mutations (message thread or IPC thread, then triggers onChanged) ─────
    void addChord(Chord chord);
    void removeChord(const juce::Uuid& id);
    void updateChord(const Chord& updated);   // replace by id
    void clear();

    // ── Reads (any thread, shared lock) ──────────────────────────────────────
    std::vector<Chord> getChordsCopy() const;

    // Returns copies — callers should not hold pointers into the internal vector
    std::optional<Chord> chordAt(FullTimeCode ppq) const;
    std::optional<Chord> chordById(const juce::Uuid& id) const;
    std::optional<Chord> prevChord(const juce::Uuid& id) const;
    std::optional<Chord> nextChord(const juce::Uuid& id) const;

    // Returns the three chords relevant for the heatmap context at ppq:
    // [current, prev, next] — any of which may be nullopt
    struct Context {
        std::optional<Chord> current, prev, next;
    };
    Context contextAt(FullTimeCode ppq) const;

    // ── Change notification (message thread) ─────────────────────────────────
    // Called after every mutation. Set by owner (PluginProcessor / SessionManager).
    std::function<void()> onChanged;

    // ── Serialization ─────────────────────────────────────────────────────────
    juce::var      toVar()       const;
    void           fromVar(const juce::var& v);
    juce::ValueTree toValueTree() const;
    void            fromValueTree(const juce::ValueTree& vt);

private:
    mutable std::shared_mutex mutex_;
    std::vector<Chord> chords_;   // sorted by start time

    void sortLocked();            // call while holding unique_lock
    void notifyChanged();
};
