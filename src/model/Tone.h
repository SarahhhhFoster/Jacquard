#pragma once
#include "FullTimeCode.h"
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

struct Tone
{
    juce::Uuid       id;
    juce::Uuid       sessionId;   // which plugin instance created this tone
    double           frequency  = 440.0; // Hz, arbitrary precision
    FullTimeCode     start      = 0.0;   // PPQ
    FullTimeCode     stop       = 0.0;   // PPQ  (must be > start)
    bool             enabled    = true;  // per-session enable/disable

    bool isValid() const { return stop > start && frequency > 0.0; }

    // ── Serialization ────────────────────────────────────────────────────────
    juce::var toVar() const
    {
        auto obj = new juce::DynamicObject();
        obj->setProperty("id",        id.toString());
        obj->setProperty("sessionId", sessionId.toString());
        obj->setProperty("frequency", frequency);
        obj->setProperty("start",     start);
        obj->setProperty("stop",      stop);
        obj->setProperty("enabled",   enabled);
        return juce::var(obj);
    }

    static Tone fromVar(const juce::var& v)
    {
        Tone t;
        t.id        = juce::Uuid(v["id"].toString());
        t.sessionId = juce::Uuid(v["sessionId"].toString());
        t.frequency = (double) v["frequency"];
        t.start     = (double) v["start"];
        t.stop      = (double) v["stop"];
        t.enabled   = (bool)   v["enabled"];
        return t;
    }

    // ── ValueTree round-trip (for DAW state) ─────────────────────────────────
    juce::ValueTree toValueTree() const
    {
        juce::ValueTree vt("Tone");
        vt.setProperty("id",        id.toString(),        nullptr);
        vt.setProperty("sessionId", sessionId.toString(), nullptr);
        vt.setProperty("frequency", frequency,            nullptr);
        vt.setProperty("start",     start,                nullptr);
        vt.setProperty("stop",      stop,                 nullptr);
        vt.setProperty("enabled",   enabled,              nullptr);
        return vt;
    }

    static Tone fromValueTree(const juce::ValueTree& vt)
    {
        Tone t;
        t.id        = juce::Uuid(vt["id"].toString());
        t.sessionId = juce::Uuid(vt["sessionId"].toString());
        t.frequency = (double) vt["frequency"];
        t.start     = (double) vt["start"];
        t.stop      = (double) vt["stop"];
        t.enabled   = (bool)   vt["enabled"];
        return t;
    }
};
