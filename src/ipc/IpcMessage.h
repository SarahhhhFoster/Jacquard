#pragma once
#include "../model/ChordTimeline.h"
#include <juce_core/juce_core.h>

// Wire protocol:
//   [4 bytes LE: payload length] [UTF-8 JSON payload]

namespace IpcMsg
{
    // Message type strings
    constexpr auto HELLO          = "HELLO";
    constexpr auto WELCOME        = "WELCOME";
    constexpr auto TIMELINE_FULL  = "TIMELINE_FULL";
    constexpr auto ADD_CHORD      = "ADD_CHORD";
    constexpr auto REMOVE_CHORD   = "REMOVE_CHORD";
    constexpr auto UPDATE_CHORD   = "UPDATE_CHORD";
    constexpr auto ADD_TONE       = "ADD_TONE";
    constexpr auto REMOVE_TONE    = "REMOVE_TONE";
    constexpr auto TOGGLE_TONE    = "TOGGLE_TONE";
    constexpr auto SERVER_RESIGN  = "SERVER_RESIGN";
    constexpr auto BYE            = "BYE";

    inline juce::String encode(const juce::var& payload)
    {
        return juce::JSON::toString(payload, true);
    }

    inline juce::var decode(const juce::String& json)
    {
        juce::var result;
        juce::JSON::parse(json, result);
        return result;
    }

    // Build typed message objects
    inline juce::var makeHello(const juce::Uuid& sessionId)
    {
        auto obj = new juce::DynamicObject();
        obj->setProperty("type",      HELLO);
        obj->setProperty("sessionId", sessionId.toString());
        obj->setProperty("version",   1);
        return juce::var(obj);
    }

    inline juce::var makeWelcome(const juce::Uuid& sessionId, const ChordTimeline& tl)
    {
        auto obj = new juce::DynamicObject();
        obj->setProperty("type",      WELCOME);
        obj->setProperty("sessionId", sessionId.toString());
        obj->setProperty("timeline",  tl.toVar());
        return juce::var(obj);
    }

    inline juce::var makeTimelineFull(const ChordTimeline& tl)
    {
        auto obj = new juce::DynamicObject();
        obj->setProperty("type",     TIMELINE_FULL);
        obj->setProperty("timeline", tl.toVar());
        return juce::var(obj);
    }

    inline juce::var makeAddChord(const Chord& chord)
    {
        auto obj = new juce::DynamicObject();
        obj->setProperty("type",  ADD_CHORD);
        obj->setProperty("chord", chord.toVar());
        return juce::var(obj);
    }

    inline juce::var makeRemoveChord(const juce::Uuid& chordId)
    {
        auto obj = new juce::DynamicObject();
        obj->setProperty("type",    REMOVE_CHORD);
        obj->setProperty("chordId", chordId.toString());
        return juce::var(obj);
    }

    inline juce::var makeUpdateChord(const Chord& chord)
    {
        auto obj = new juce::DynamicObject();
        obj->setProperty("type",  UPDATE_CHORD);
        obj->setProperty("chord", chord.toVar());
        return juce::var(obj);
    }

    inline juce::var makeAddTone(const juce::Uuid& chordId, const Tone& tone)
    {
        auto obj = new juce::DynamicObject();
        obj->setProperty("type",    ADD_TONE);
        obj->setProperty("chordId", chordId.toString());
        obj->setProperty("tone",    tone.toVar());
        return juce::var(obj);
    }

    inline juce::var makeRemoveTone(const juce::Uuid& chordId, const juce::Uuid& toneId)
    {
        auto obj = new juce::DynamicObject();
        obj->setProperty("type",    REMOVE_TONE);
        obj->setProperty("chordId", chordId.toString());
        obj->setProperty("toneId",  toneId.toString());
        return juce::var(obj);
    }

    inline juce::var makeToggleTone(const juce::Uuid& chordId,
                                    const juce::Uuid& toneId,
                                    const juce::Uuid& sessionId,
                                    bool enabled)
    {
        auto obj = new juce::DynamicObject();
        obj->setProperty("type",      TOGGLE_TONE);
        obj->setProperty("chordId",   chordId.toString());
        obj->setProperty("toneId",    toneId.toString());
        obj->setProperty("sessionId", sessionId.toString());
        obj->setProperty("enabled",   enabled);
        return juce::var(obj);
    }

    inline juce::var makeServerResign()
    {
        auto obj = new juce::DynamicObject();
        obj->setProperty("type", SERVER_RESIGN);
        return juce::var(obj);
    }

    inline juce::var makeBye(const juce::Uuid& sessionId)
    {
        auto obj = new juce::DynamicObject();
        obj->setProperty("type",      BYE);
        obj->setProperty("sessionId", sessionId.toString());
        return juce::var(obj);
    }
}
