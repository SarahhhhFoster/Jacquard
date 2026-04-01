#include "IpcClient.h"
#include <juce_events/juce_events.h>

IpcClient::IpcClient() : juce::Thread("JacquardIPCClient") {}

IpcClient::~IpcClient()
{
    disconnect();
}

bool IpcClient::connect()
{
    socket_ = std::make_unique<juce::StreamingSocket>();
    if (!socket_->connect("127.0.0.1", kPort, kConnectTimeoutMs))
    {
        socket_.reset();
        return false;
    }
    connected_ = true;
    startThread();
    return true;
}

void IpcClient::disconnect()
{
    connected_ = false;
    if (socket_) socket_->close();
    signalThreadShouldExit();
    stopThread(2000);
    socket_.reset();
}

bool IpcClient::send(const juce::String& json)
{
    if (!connected_ || !socket_) return false;
    sendFramed(json);
    return true;
}

void IpcClient::run()
{
    while (!threadShouldExit() && connected_)
    {
        // Read 4-byte LE length prefix
        uint32_t len = 0;
        int got = socket_->read(&len, 4, true);
        if (got != 4) break;

        uint32_t lenLE = (uint32_t)(
            (uint32_t)((const uint8_t*)&len)[0]        |
            ((uint32_t)((const uint8_t*)&len)[1] << 8)  |
            ((uint32_t)((const uint8_t*)&len)[2] << 16) |
            ((uint32_t)((const uint8_t*)&len)[3] << 24));

        if (lenLE == 0 || lenLE > 1024 * 1024) break;

        juce::MemoryBlock buf(lenLE);
        got = socket_->read(buf.getData(), (int)lenLE, true);
        if (got != (int)lenLE) break;

        juce::String json(static_cast<const char*>(buf.getData()), lenLE);
        if (onMessage) onMessage(json);
    }

    connected_ = false;
    if (onDisconnected)
        juce::MessageManager::callAsync(onDisconnected);
}

void IpcClient::sendFramed(const juce::String& json)
{
    if (!socket_) return;
    const char* data = json.toRawUTF8();
    uint32_t    size = (uint32_t)json.getNumBytesAsUTF8();

    uint8_t header[4] = {
        (uint8_t)(size & 0xFF),
        (uint8_t)((size >> 8) & 0xFF),
        (uint8_t)((size >> 16) & 0xFF),
        (uint8_t)((size >> 24) & 0xFF)
    };
    socket_->write(header, 4);
    socket_->write(data, (int)size);
}
