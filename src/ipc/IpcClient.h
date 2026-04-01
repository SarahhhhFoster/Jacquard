#pragma once
#include <juce_core/juce_core.h>
#include <functional>
#include <atomic>

// IpcClient connects to the IpcServer and exchanges framed JSON messages.
// It runs its read loop on a background thread and surfaces received messages
// via the onMessage callback (called on the IPC thread).

class IpcClient : private juce::Thread
{
public:
    static constexpr int kPort             = 52847;
    static constexpr int kConnectTimeoutMs = 500;

    IpcClient();
    ~IpcClient() override;

    // Attempt a single connection.  Returns true on success.
    bool connect();
    void disconnect();
    bool isConnected() const { return connected_; }

    // Send a JSON string to the server.
    bool send(const juce::String& json);

    // Callbacks (invoked on IPC thread)
    std::function<void(const juce::String& json)> onMessage;
    std::function<void()>                          onDisconnected;

private:
    void run() override;
    void sendFramed(const juce::String& json);

    std::unique_ptr<juce::StreamingSocket> socket_;
    std::atomic<bool>                      connected_{ false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IpcClient)
};
