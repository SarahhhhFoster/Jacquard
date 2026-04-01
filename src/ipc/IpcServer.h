#pragma once
#include <juce_core/juce_core.h>
#include <functional>
#include <vector>
#include <memory>
#include <mutex>

// IpcServer listens on a TCP port and accepts connections from IpcClient instances.
// It broadcasts outgoing messages to all connected clients and calls onMessage
// for each inbound message.

class IpcServer : private juce::Thread
{
public:
    static constexpr int kPort = 52847;

    IpcServer();
    ~IpcServer() override;

    // Start listening.  Returns false if port is already in use.
    bool start();
    void stop();

    // Send a JSON string to all connected clients.
    void broadcast(const juce::String& json);

    // Called on the IPC thread for every inbound message.
    std::function<void(const juce::String& json)> onMessage;

private:
    void run() override;
    void handleClient(std::unique_ptr<juce::StreamingSocket> sock);
    void sendFramed(juce::StreamingSocket& sock, const juce::String& json);

    struct ClientConn
    {
        std::unique_ptr<juce::StreamingSocket> socket;
        std::unique_ptr<juce::Thread>          thread;
        std::atomic<bool>                      alive{ true };
    };

    std::unique_ptr<juce::StreamingSocket> listenSock_;
    std::vector<std::shared_ptr<ClientConn>> clients_;
    std::mutex clientsMutex_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IpcServer)
};
