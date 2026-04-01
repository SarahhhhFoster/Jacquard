#pragma once
#include "IpcServer.h"
#include "IpcClient.h"
#include "IpcMessage.h"
#include "../model/ChordTimeline.h"
#include <juce_core/juce_core.h>
#include <functional>
#include <memory>
#include <random>

// SessionManager handles the server-election protocol and keeps the local
// ChordTimeline in sync with all other plugin instances on this machine.
//
// Election:
//   1. Try to connect as a client.
//   2. On failure, try to start a server.
//   3. On failure (another instance just won), retry as client.
//
// All mutations to the timeline (from UI) should go through SessionManager so
// they are propagated to other instances.

class SessionManager : private juce::Timer
{
public:
    explicit SessionManager(ChordTimeline& timeline);
    ~SessionManager() override;

    // Start the session (attempt election). Non-blocking.
    void start();
    void stop();

    bool isServer() const { return server_ != nullptr; }
    const juce::Uuid& sessionId() const { return sessionId_; }

    // Outbound operations — send to peers and apply locally.
    void addChord(const Chord& c);
    void removeChord(const juce::Uuid& chordId);
    void updateChord(const Chord& c);
    void addTone(const juce::Uuid& chordId, const Tone& t);
    void removeTone(const juce::Uuid& chordId, const juce::Uuid& toneId);
    void toggleTone(const juce::Uuid& chordId, const juce::Uuid& toneId, bool enabled);

    // Called on message thread when session state changes (any mutation).
    std::function<void()> onSessionChanged;

    // Called only when tones are structurally added/removed/toggled —
    // used to trigger heatmap recomputation without doing so on every
    // chord move or resize.
    std::function<void()> onToneStructural;

private:
    void timerCallback() override;
    void tryElection();
    void becomeServer();
    void becomeClient();

    void handleMessage(const juce::String& json);
    void broadcast(const juce::var& msg);
    void send(const juce::var& msg);

    ChordTimeline&             timeline_;
    juce::Uuid                 sessionId_;
    std::unique_ptr<IpcServer> server_;
    std::unique_ptr<IpcClient> client_;

    enum class State { Idle, TryingClient, TryingServer, Running };
    State state_{ State::Idle };
    int   electionAttempts_{ 0 };

    // Jitter seed for re-election backoff
    std::mt19937 rng_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SessionManager)
};
