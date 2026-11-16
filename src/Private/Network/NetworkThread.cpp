#include "NetworkThread.h"

#include <enet/enet.h>
#include <spdlog/spdlog.h>

namespace fc::Network
{

NetworkThread::~NetworkThread()
{
    Stop();
}

bool NetworkThread::Start()
{
    if (mThread.joinable())
    {
        return false;
    }

    mThread = std::thread(&NetworkThread::Main, this);

    return true;
}

void NetworkThread::Stop()
{
    mOutgoingCV.notify_all();

    if (mThread.joinable())
    {
        mThread.join();
    }
}

bool NetworkThread::PollEvent(Event& outEvent)
{
    std::lock_guard<std::mutex> lock(mEventMutex);
    if (mEventQueue.empty()) return false;

    outEvent = mEventQueue.front();
    mEventQueue.pop();
    return true;
}

bool NetworkThread::TryPopECSMessage(ECS::Message& outMessage)
{
    std::lock_guard<std::mutex> lock(mIncomingMutex);
    if (mIncomingMessages.empty()) return false;

    outMessage = std::move(mIncomingMessages.front());
    mIncomingMessages.pop();
    return true;
}

void NetworkThread::QueueMessageTo(const std::vector<uint8_t>& data, ENetPeer* peer, const Channel channel, const uint32_t flags)
{
    std::lock_guard<std::mutex> lock(mOutgoingMutex);

    mOutgoingMessages.push({
        .mData = data,
        .mPeer = peer,
        .mChannel = channel,
        .mFlags = flags
    });
}

void NetworkThread::QueueECSMessageTo(const ECS::Message& message, ENetPeer* peer)
{
    const std::vector<uint8_t> packet = message.Serialise();
    QueueMessageTo(packet, peer, Channel::Messaging, ENET_PACKET_FLAG_RELIABLE);
}

void NetworkThread::Main()
{
    if (enet_initialize() != 0)
    {
        spdlog::error("Failed to initialize ENet");
        return;
    }

    if (!Init())
    {
        enet_deinitialize();
        return;
    }

    State state = mState.load();
    ENetEvent event;
    while (state != PendingExit)
    {
        if (state == Idle)
        {
            state = mState.load();
            continue;
        }

        ProcessOutgoingMessages();
        Update();

        int result = enet_host_service(mHost, &event, 16);
        if (result > 0)
        {
            HandleEvent(event);
        }
        else if (result < 0)
        {
            spdlog::error("An ENet service error occurred");
            break;
        }

        state = mState.load();
    }

    Disconnect();

    if (mHost)
    {
        enet_host_destroy(mHost);
    }

    enet_deinitialize();
}

void NetworkThread::ProcessOutgoingMessages()
{
    // TODO: add tuneable throttling

    // Drain under lock so senders on other threads can keep queueing while we send
    std::queue<OutboundMessage> messages;
    {
        std::lock_guard<std::mutex> lock(mOutgoingMutex);
        messages.swap(mOutgoingMessages);
    }

    while (!messages.empty())
    {
        OutboundMessage& message = messages.front();

        if (message.mPeer == nullptr)
        {
            spdlog::warn("Dropping a {} byte message - there is no peer to send it to.", message.mData.size());
            messages.pop();
            continue;
        }

        auto* data = new uint8_t[message.mData.size()];
        std::copy(message.mData.begin(), message.mData.end(), data);

        ENetPacket* packet = enet_packet_create(data, message.mData.size(), message.mFlags | ENET_PACKET_FLAG_NO_ALLOCATE);
        if (packet == nullptr)
        {
            delete[] data;
            messages.pop();
            continue;
        }

        packet->freeCallback = [](ENetPacket* p)
        {
            delete[] p->data;
            p->data = nullptr;
        };

        // Doesn't take ownership of the packet on failure (e.g. peer not connected)
        if (enet_peer_send(message.mPeer, message.mChannel, packet) != 0)
        {
            spdlog::warn("Failed to enqueue a {} byte message on channel {} for sending; dropping it.",
                message.mData.size(), message.mChannel);
            enet_packet_destroy(packet);
        }

        messages.pop();
    }
}

void NetworkThread::HandleEvent(const ENetEvent& event)
{
    switch (event.type)
    {
        case ENET_EVENT_TYPE_CONNECT:
        {
            std::lock_guard<std::mutex> lock(mEventMutex);
            mEventQueue.push({Event::Type::PeerConnect, event.peer});

            break;
        }
        case ENET_EVENT_TYPE_DISCONNECT:
        {
            std::lock_guard<std::mutex> lock(mEventMutex);
            mEventQueue.push({Event::Type::PeerDisconnect, event.peer});

            break;
        }
        case ENET_EVENT_TYPE_RECEIVE:
        {
            switch (event.channelID)
            {
                case Replication:
                {
                    // Owned by ClientThread::HandleEvent, which destroys the packet
                    break;
                }

                case Messaging:
                {
                    ECS::Message message;
                    if (!ECS::Message::Deserialise(event.packet->data, event.packet->dataLength, message))
                    {
                        spdlog::warn("Received a malformed message packet ({} bytes); dropping it.", event.packet->dataLength);
                    }
                    else
                    {
                        message.mSender = event.peer;

                        std::lock_guard<std::mutex> lock(mIncomingMutex);
                        mIncomingMessages.push(std::move(message));
                    }

                    enet_packet_destroy(event.packet);

                    break;
                }

                default:
                {
                    spdlog::warn("Received a packet on unknown channel {}; dropping it.", event.channelID);
                    enet_packet_destroy(event.packet);

                    break;
                }
            }

            break;
        }

        default:
            break;
    }
}

} // namespace fc::Network
