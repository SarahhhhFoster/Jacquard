#include "IpcServer.h"
#include <juce_events/juce_events.h>

IpcServer::IpcServer() : juce::Thread("JacquardIPCServer") {}

IpcServer::~IpcServer()
{
    stop();
}

bool IpcServer::start()
{
    listenSock_ = std::make_unique<juce::StreamingSocket>();
    if (!listenSock_->createListener(kPort))
    {
        listenSock_.reset();
        return false;
    }
    startThread();
    return true;
}

void IpcServer::stop()
{
    signalThreadShouldExit();
    if (listenSock_)
        listenSock_->close();
    stopThread(2000);

    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto& c : clients_)
    {
        c->alive = false;
        if (c->socket) c->socket->close();
        if (c->thread) c->thread->stopThread(500);
    }
    clients_.clear();
}

void IpcServer::broadcast(const juce::String& json)
{
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto& c : clients_)
        if (c->alive && c->socket)
            sendFramed(*c->socket, json);
}

void IpcServer::run()
{
    while (!threadShouldExit())
    {
        auto clientSock = std::unique_ptr<juce::StreamingSocket>(
            listenSock_->waitForNextConnection());

        if (!clientSock || threadShouldExit()) break;

        auto conn = std::make_shared<ClientConn>();
        conn->socket = std::move(clientSock);

        // Capture a weak reference so the thread doesn't hold the conn alive
        auto weakConn = std::weak_ptr<ClientConn>(conn);
        auto cb = onMessage;

        // We can't pass a lambda to juce::Thread directly, so we use a subclass
        struct ReaderThread : public juce::Thread
        {
            std::weak_ptr<ClientConn> conn;
            std::function<void(const juce::String&)> onMsg;
            IpcServer* server;

            ReaderThread(std::weak_ptr<ClientConn> c,
                         std::function<void(const juce::String&)> f,
                         IpcServer* s)
                : juce::Thread("JacquardIPCReader"), conn(c), onMsg(f), server(s) {}

            void run() override
            {
                while (!threadShouldExit())
                {
                    auto sp = conn.lock();
                    if (!sp || !sp->alive) break;

                    // Read 4-byte length prefix
                    uint32_t len = 0;
                    int got = sp->socket->read(&len, 4, true);
                    if (got != 4) { sp->alive = false; break; }

                    len = juce::ByteOrder::littleEndianInt(reinterpret_cast<const uint8_t*>(&len));
                    if (len == 0 || len > 1024 * 1024) { sp->alive = false; break; }

                    juce::MemoryBlock buf(len);
                    got = sp->socket->read(buf.getData(), (int)len, true);
                    if (got != (int)len) { sp->alive = false; break; }

                    juce::String json(static_cast<const char*>(buf.getData()), len);
                    if (onMsg) onMsg(json);
                }

                // Clean up dead connection on message thread
                auto sp = conn.lock();
                juce::MessageManager::callAsync([server = server, sp]()
                {
                    if (!server) return;
                    std::lock_guard<std::mutex> lock(server->clientsMutex_);
                    server->clients_.erase(
                        std::remove_if(server->clients_.begin(), server->clients_.end(),
                            [](const auto& c){ return !c->alive; }),
                        server->clients_.end());
                });
            }
        };

        auto* reader = new ReaderThread(weakConn, cb, this);
        conn->thread.reset(reader);
        reader->startThread();

        std::lock_guard<std::mutex> lock(clientsMutex_);
        clients_.push_back(std::move(conn));
    }
}

void IpcServer::sendFramed(juce::StreamingSocket& sock, const juce::String& json)
{
    juce::MemoryBlock utf8;
    utf8.setSize(0);
    const char* data = json.toRawUTF8();
    size_t      size = json.getNumBytesAsUTF8();

    uint32_t len = juce::ByteOrder::littleEndianInt(
        reinterpret_cast<const uint8_t*>(&size));
    // Note: we write size as little-endian 32-bit
    uint32_t lenLE = (uint32_t)size;
    // Use platform-independent little-endian write
    uint8_t header[4] = {
        (uint8_t)(lenLE & 0xFF),
        (uint8_t)((lenLE >> 8) & 0xFF),
        (uint8_t)((lenLE >> 16) & 0xFF),
        (uint8_t)((lenLE >> 24) & 0xFF)
    };

    sock.write(header, 4);
    sock.write(data, (int)size);
}
