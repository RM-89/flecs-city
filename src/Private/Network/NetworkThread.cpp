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

void NetworkThread::QueueMessage(const std::string& data, ENetPeer* peer, const Channel channel, const uint32_t flags)
{
    const std::vector<uint8_t> vec(data.begin(), data.end());
    QueueMessage(vec, peer, channel, flags);
}

void NetworkThread::QueueMessage(const std::vector<uint8_t>& data, ENetPeer* peer, const Channel channel, const uint32_t flags)
{
    mOutgoingMessages.push({
        .mData = data,
        .mPeer = peer,
        .mChannel = channel,
        .mFlags = flags
    });
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
    while (!mOutgoingMessages.empty())
    {
        OutMessage& message = mOutgoingMessages.front();

        auto* data = new uint8_t[message.mData.size()];
        std::copy(message.mData.begin(), message.mData.end(), data);

        ENetPacket* packet = enet_packet_create(data, message.mData.size(), ENET_PACKET_FLAG_RELIABLE | ENET_PACKET_FLAG_NO_ALLOCATE);
        packet->freeCallback = [](ENetPacket* p)
        {
            delete[] p->data;
            p->data = nullptr;
        };

        enet_peer_send(message.mPeer, message.mChannel, packet);

        mOutgoingMessages.pop();
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
                case General:
                {
                    std::vector<uint8_t> data(event.packet->data, event.packet->data + event.packet->dataLength);

                    std::string msg(data.begin(), data.end());
                    spdlog::info("Queuing incoming message: {}", msg);

                    {
                        std::lock_guard<std::mutex> lock(mIncomingMutex);
                        mIncomingMessages.push({std::move(data), event.channelID});
                    }

                    enet_packet_destroy(event.packet);

                    break;
                }
            }
        }

        default:
            break;
    }
}

} // namespace fc::Network
