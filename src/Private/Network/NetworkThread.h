#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include <enet/enet.h>

#include "ECS/Message.h"
#include "Network/Types.h"

namespace fc::Network
{

enum Channel : uint8_t
{
    Replication = 0,
    Messaging = 1
};

struct Event
{
    enum Type
    {
        PeerConnect,
        PeerDisconnect
    };

    Type mType;
    ENetPeer* mPeer;
};

class NetworkThread
{
public:
    enum State
    {
        Idle,
        Active,
        PendingExit
    };

    NetworkThread() = default;
    ~NetworkThread();

    bool Start();
    void Stop();

    bool PollEvent(Event& outEvent);

    /// @brief Pops the next message that was received on @c fc::Network::Channel::Messaging (if any).
    /// @return True if a message was popped.
    bool TryPopECSMessage(ECS::Message& outMessage);

    /// @brief Queues a message for sending on @c fc::Network::Channel::Messaging.
    ///
    /// - In server mode, queued messages are sent to specific clients when specified or broadcast to all clients otherwise.
    /// - In client mode, queued messages are sent to the server.
    virtual void QueueECSMessage(const ECS::Message& message) = 0;

protected:
    std::thread mThread;
    std::atomic<State> mState{State::Idle};

    std::atomic<ENetHost*> mHost{nullptr};

    /// @brief Connection address (when this is a client) or listen address (when this is a server).
    ENetAddress mAddress;

    std::mutex mIncomingMutex;
    std::mutex mOutgoingMutex;
    std::condition_variable mOutgoingCV;

    std::queue<ECS::Message> mIncomingMessages;
    std::queue<OutboundMessage> mOutgoingMessages;

    void QueueMessageTo(const std::vector<uint8_t>& data, ENetPeer* peer, Channel channel, uint32_t flags);

    /// @brief Serialises a @c fc::Network::Message and queues it for sending to a single peer.
    void QueueECSMessageTo(const ECS::Message& message, ENetPeer* peer);

    virtual bool Init() = 0;
    virtual void HandleEvent(const ENetEvent& event);
    virtual void Update() {}
    virtual void Disconnect() {}

private:
    std::mutex mEventMutex;
    std::queue<Event> mEventQueue;

    /// @brief Thread function
    void Main();

    void ProcessOutgoingMessages();
};

} // namespace fc::Network
