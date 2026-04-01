#pragma once
#include "Tone.h"
#include <vector>
#include <algorithm>

struct Chord
{
    juce::Uuid       id;
    FullTimeCode     start = 0.0;
    FullTimeCode     stop  = 0.0;
    std::vector<Tone> tones;

    bool isValid()      const { return stop > start; }
    bool containsPPQ(FullTimeCode ppq) const { return ppq >= start && ppq < stop; }
    double duration()   const { return stop - start; }

    // Tones must be clamped to chord bounds on insertion or chord resize.
    void clampTone(Tone& t) const
    {
        t.start = juce::jlimit(start, stop, t.start);
        t.stop  = juce::jlimit(start, stop, t.stop);
        if (t.stop <= t.start)
            t.stop = t.start + 0.001;
    }

    void addTone(Tone t)
    {
        clampTone(t);
        tones.push_back(std::move(t));
    }

    bool removeTone(const juce::Uuid& toneId)
    {
        auto it = std::find_if(tones.begin(), tones.end(),
                               [&](const Tone& t){ return t.id == toneId; });
        if (it == tones.end()) return false;
        tones.erase(it);
        return true;
    }

    Tone* findTone(const juce::Uuid& toneId)
    {
        auto it = std::find_if(tones.begin(), tones.end(),
                               [&](const Tone& t){ return t.id == toneId; });
        return it == tones.end() ? nullptr : &(*it);
    }

    // Frequencies of all tones (for consonance engine)
    std::vector<double> frequencies() const
    {
        std::vector<double> f;
        f.reserve(tones.size());
        for (const auto& t : tones)
            if (t.enabled) f.push_back(t.frequency);
        return f;
    }

    // ── Serialization ────────────────────────────────────────────────────────
    juce::var toVar() const
    {
        auto obj = new juce::DynamicObject();
        obj->setProperty("id",    id.toString());
        obj->setProperty("start", start);
        obj->setProperty("stop",  stop);
        juce::Array<juce::var> tArr;
        for (const auto& t : tones)
            tArr.add(t.toVar());
        obj->setProperty("tones", tArr);
        return juce::var(obj);
    }

    static Chord fromVar(const juce::var& v)
    {
        Chord c;
        c.id    = juce::Uuid(v["id"].toString());
        c.start = (double) v["start"];
        c.stop  = (double) v["stop"];
        if (const auto* arr = v["tones"].getArray())
            for (const auto& tv : *arr)
                c.tones.push_back(Tone::fromVar(tv));
        return c;
    }

    juce::ValueTree toValueTree() const
    {
        juce::ValueTree vt("Chord");
        vt.setProperty("id",    id.toString(), nullptr);
        vt.setProperty("start", start,         nullptr);
        vt.setProperty("stop",  stop,           nullptr);
        for (const auto& t : tones)
            vt.addChild(t.toValueTree(), -1, nullptr);
        return vt;
    }

    static Chord fromValueTree(const juce::ValueTree& vt)
    {
        Chord c;
        c.id    = juce::Uuid(vt["id"].toString());
        c.start = (double) vt["start"];
        c.stop  = (double) vt["stop"];
        for (int i = 0; i < vt.getNumChildren(); ++i)
            c.tones.push_back(Tone::fromValueTree(vt.getChild(i)));
        return c;
    }
};
