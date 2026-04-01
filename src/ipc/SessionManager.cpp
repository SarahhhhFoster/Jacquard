#include "SessionManager.h"

SessionManager::SessionManager(ChordTimeline& timeline)
    : timeline_(timeline)
    , sessionId_()
    , rng_(std::random_device{}())
{
}

SessionManager::~SessionManager()
{
    stop();
}

void SessionManager::start()
{
    state_ = State::Idle;
    electionAttempts_ = 0;
    startTimer(50); // kick off election after 50ms
}

void SessionManager::stop()
{
    stopTimer();
    if (client_)
    {
        client_->send(IpcMsg::encode(IpcMsg::makeBye(sessionId_)));
        client_->disconnect();
        client_.reset();
    }
    if (server_)
    {
        server_->broadcast(IpcMsg::encode(IpcMsg::makeServerResign()));
        server_->stop();
        server_.reset();
    }
    state_ = State::Idle;
}

// ── Outbound mutations ────────────────────────────────────────────────────────

void SessionManager::addChord(const Chord& c)
{
    timeline_.addChord(c);
    broadcast(IpcMsg::makeAddChord(c));
    if (onToneStructural) juce::MessageManager::callAsync(onToneStructural);
}

void SessionManager::removeChord(const juce::Uuid& id)
{
    timeline_.removeChord(id);
    broadcast(IpcMsg::makeRemoveChord(id));
    if (onToneStructural) juce::MessageManager::callAsync(onToneStructural);
}

void SessionManager::updateChord(const Chord& c)
{
    timeline_.updateChord(c);
    broadcast(IpcMsg::makeUpdateChord(c));
}

void SessionManager::addTone(const juce::Uuid& chordId, const Tone& t)
{
    auto chord = timeline_.chordById(chordId);
    if (!chord) return;
    chord->addTone(t);
    timeline_.updateChord(*chord);
    broadcast(IpcMsg::makeAddTone(chordId, t));
    if (onToneStructural) juce::MessageManager::callAsync(onToneStructural);
}

void SessionManager::removeTone(const juce::Uuid& chordId, const juce::Uuid& toneId)
{
    auto chord = timeline_.chordById(chordId);
    if (!chord) return;
    chord->removeTone(toneId);
    timeline_.updateChord(*chord);
    broadcast(IpcMsg::makeRemoveTone(chordId, toneId));
    if (onToneStructural) juce::MessageManager::callAsync(onToneStructural);
}

void SessionManager::toggleTone(const juce::Uuid& chordId, const juce::Uuid& toneId, bool enabled)
{
    auto chord = timeline_.chordById(chordId);
    if (!chord) return;
    if (auto* tone = chord->findTone(toneId))
    {
        tone->enabled = enabled;
        timeline_.updateChord(*chord);
        broadcast(IpcMsg::makeToggleTone(chordId, toneId, sessionId_, enabled));
        if (onToneStructural) juce::MessageManager::callAsync(onToneStructural);
    }
}

// ── Timer: election state machine ─────────────────────────────────────────────

void SessionManager::timerCallback()
{
    stopTimer();
    tryElection();
}

void SessionManager::tryElection()
{
    // 1. Try to connect as client
    auto testClient = std::make_unique<IpcClient>();
    if (testClient->connect())
    {
        client_ = std::move(testClient);
        becomeClient();
        return;
    }

    // 2. Try to start server
    auto testServer = std::make_unique<IpcServer>();
    if (testServer->start())
    {
        server_ = std::move(testServer);
        becomeServer();
        return;
    }

    // 3. Both failed (race) — retry with jitter
    ++electionAttempts_;
    std::uniform_int_distribution<int> jitter(50, 200);
    startTimer(jitter(rng_));
}

void SessionManager::becomeServer()
{
    server_->onMessage = [this](const juce::String& json) { handleMessage(json); };
    state_ = State::Running;
    if (onSessionChanged)
        juce::MessageManager::callAsync(onSessionChanged);
}

void SessionManager::becomeClient()
{
    client_->onMessage = [this](const juce::String& json) { handleMessage(json); };
    client_->onDisconnected = [this]()
    {
        // Server resigned — re-run election
        client_.reset();
        state_ = State::Idle;
        std::uniform_int_distribution<int> jitter(50, 200);
        startTimer(jitter(rng_));
        if (onSessionChanged) onSessionChanged();
    };

    // Announce ourselves
    client_->send(IpcMsg::encode(IpcMsg::makeHello(sessionId_)));
    state_ = State::Running;
    if (onSessionChanged)
        juce::MessageManager::callAsync(onSessionChanged);
}

// ── Inbound message handler ───────────────────────────────────────────────────

void SessionManager::handleMessage(const juce::String& json)
{
    auto msg = IpcMsg::decode(json);
    auto type = msg["type"].toString();

    if (type == IpcMsg::HELLO)
    {
        // A new client connected to us (server role) — send them the full timeline
        if (server_)
            server_->broadcast(IpcMsg::encode(IpcMsg::makeTimelineFull(timeline_)));
    }
    else if (type == IpcMsg::TIMELINE_FULL || type == IpcMsg::WELCOME)
    {
        timeline_.fromVar(msg["timeline"]);
    }
    else if (type == IpcMsg::ADD_CHORD)
    {
        auto chord = Chord::fromVar(msg["chord"]);
        timeline_.addChord(chord);
        if (server_) server_->broadcast(json);
        if (onToneStructural) juce::MessageManager::callAsync(onToneStructural);
    }
    else if (type == IpcMsg::REMOVE_CHORD)
    {
        timeline_.removeChord(juce::Uuid(msg["chordId"].toString()));
        if (server_) server_->broadcast(json);
        if (onToneStructural) juce::MessageManager::callAsync(onToneStructural);
    }
    else if (type == IpcMsg::UPDATE_CHORD)
    {
        timeline_.updateChord(Chord::fromVar(msg["chord"]));
        if (server_) server_->broadcast(json);
    }
    else if (type == IpcMsg::ADD_TONE)
    {
        juce::Uuid chordId(msg["chordId"].toString());
        auto tone  = Tone::fromVar(msg["tone"]);
        auto chord = timeline_.chordById(chordId);
        if (chord) { chord->addTone(tone); timeline_.updateChord(*chord); }
        if (server_) server_->broadcast(json);
        if (onToneStructural) juce::MessageManager::callAsync(onToneStructural);
    }
    else if (type == IpcMsg::REMOVE_TONE)
    {
        juce::Uuid chordId(msg["chordId"].toString());
        juce::Uuid toneId(msg["toneId"].toString());
        auto chord = timeline_.chordById(chordId);
        if (chord) { chord->removeTone(toneId); timeline_.updateChord(*chord); }
        if (server_) server_->broadcast(json);
        if (onToneStructural) juce::MessageManager::callAsync(onToneStructural);
    }
    else if (type == IpcMsg::TOGGLE_TONE)
    {
        juce::Uuid chordId(msg["chordId"].toString());
        juce::Uuid toneId(msg["toneId"].toString());
        bool enabled = (bool)msg["enabled"];
        auto chord = timeline_.chordById(chordId);
        if (chord)
        {
            if (auto* tone = chord->findTone(toneId))
            {
                tone->enabled = enabled;
                timeline_.updateChord(*chord);
            }
        }
        if (server_) server_->broadcast(json);
        if (onToneStructural) juce::MessageManager::callAsync(onToneStructural);
    }
    else if (type == IpcMsg::SERVER_RESIGN)
    {
        // Trigger re-election
        client_.reset();
        state_ = State::Idle;
        std::uniform_int_distribution<int> jitter(50, 200);
        startTimer(jitter(rng_));
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void SessionManager::broadcast(const juce::var& msg)
{
    auto json = IpcMsg::encode(msg);
    if (server_)
        server_->broadcast(json);
    else if (client_)
        client_->send(json);
}

void SessionManager::send(const juce::var& msg)
{
    if (client_)
        client_->send(IpcMsg::encode(msg));
}
