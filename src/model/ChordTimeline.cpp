#include "ChordTimeline.h"
#include <algorithm>

// ── Mutations ─────────────────────────────────────────────────────────────────

void ChordTimeline::addChord(Chord chord)
{
    {
        std::unique_lock lock(mutex_);
        chords_.push_back(std::move(chord));
        sortLocked();
    }
    notifyChanged();
}

void ChordTimeline::removeChord(const juce::Uuid& id)
{
    {
        std::unique_lock lock(mutex_);
        chords_.erase(
            std::remove_if(chords_.begin(), chords_.end(),
                           [&](const Chord& c){ return c.id == id; }),
            chords_.end());
    }
    notifyChanged();
}

void ChordTimeline::updateChord(const Chord& updated)
{
    {
        std::unique_lock lock(mutex_);
        for (auto& c : chords_)
        {
            if (c.id == updated.id)
            {
                c = updated;
                break;
            }
        }
        sortLocked();
    }
    notifyChanged();
}

void ChordTimeline::clear()
{
    {
        std::unique_lock lock(mutex_);
        chords_.clear();
    }
    notifyChanged();
}

// ── Reads ─────────────────────────────────────────────────────────────────────

std::vector<Chord> ChordTimeline::getChordsCopy() const
{
    std::shared_lock lock(mutex_);
    return chords_;
}

std::optional<Chord> ChordTimeline::chordAt(FullTimeCode ppq) const
{
    std::shared_lock lock(mutex_);
    for (const auto& c : chords_)
        if (c.containsPPQ(ppq))
            return c;
    return std::nullopt;
}

std::optional<Chord> ChordTimeline::chordById(const juce::Uuid& id) const
{
    std::shared_lock lock(mutex_);
    for (const auto& c : chords_)
        if (c.id == id)
            return c;
    return std::nullopt;
}

std::optional<Chord> ChordTimeline::prevChord(const juce::Uuid& id) const
{
    std::shared_lock lock(mutex_);
    for (int i = 1; i < (int)chords_.size(); ++i)
        if (chords_[i].id == id)
            return chords_[i - 1];
    return std::nullopt;
}

std::optional<Chord> ChordTimeline::nextChord(const juce::Uuid& id) const
{
    std::shared_lock lock(mutex_);
    for (int i = 0; i + 1 < (int)chords_.size(); ++i)
        if (chords_[i].id == id)
            return chords_[i + 1];
    return std::nullopt;
}

ChordTimeline::Context ChordTimeline::contextAt(FullTimeCode ppq) const
{
    std::shared_lock lock(mutex_);
    Context ctx;

    for (int i = 0; i < (int)chords_.size(); ++i)
    {
        if (chords_[i].containsPPQ(ppq))
        {
            ctx.current = chords_[i];
            if (i > 0)                          ctx.prev = chords_[i - 1];
            if (i + 1 < (int)chords_.size())    ctx.next = chords_[i + 1];
            break;
        }
    }
    return ctx;
}

// ── Serialization ─────────────────────────────────────────────────────────────

juce::var ChordTimeline::toVar() const
{
    std::shared_lock lock(mutex_);
    juce::Array<juce::var> arr;
    for (const auto& c : chords_)
        arr.add(c.toVar());
    return juce::var(arr);
}

void ChordTimeline::fromVar(const juce::var& v)
{
    {
        std::unique_lock lock(mutex_);
        chords_.clear();
        if (const auto* arr = v.getArray())
            for (const auto& cv : *arr)
                chords_.push_back(Chord::fromVar(cv));
        sortLocked();
    }
    notifyChanged();
}

juce::ValueTree ChordTimeline::toValueTree() const
{
    std::shared_lock lock(mutex_);
    juce::ValueTree vt("ChordTimeline");
    for (const auto& c : chords_)
        vt.addChild(c.toValueTree(), -1, nullptr);
    return vt;
}

void ChordTimeline::fromValueTree(const juce::ValueTree& vt)
{
    {
        std::unique_lock lock(mutex_);
        chords_.clear();
        for (int i = 0; i < vt.getNumChildren(); ++i)
            chords_.push_back(Chord::fromValueTree(vt.getChild(i)));
        sortLocked();
    }
    notifyChanged();
}

// ── Private ───────────────────────────────────────────────────────────────────

void ChordTimeline::sortLocked()
{
    std::sort(chords_.begin(), chords_.end(),
              [](const Chord& a, const Chord& b){ return a.start < b.start; });
}

void ChordTimeline::notifyChanged()
{
    if (onChanged)
        juce::MessageManager::callAsync([cb = onChanged]{ cb(); });
}
