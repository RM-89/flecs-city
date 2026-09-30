#include "ClientThread.h"

#include <cstdlib>

#include <enet/enet.h>
#include <spdlog/spdlog.h>

#include "ECS/ComponentRegistry.h"

namespace fc::Network
{

bool ClientThread::Connect(const char* address, uint32_t port)
{
    // TODO: Handle case where we're already connected

    enet_address_set_host(&mAddress, address);
    mAddress.port = port;

    mPeer = enet_host_connect(mHost, &mAddress, 2, 0);
    if (mPeer == nullptr)
    {
        spdlog::error("No available peers for initiating an ENet connection.");
        return false;
    }

    ENetEvent event;
    if (enet_host_service(mHost, &event, TIMEOUT_CONNECT) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
    {
        spdlog::info("Connection to {}:{} succeeded.", address, port);
        mState = State::Active;
        return true;
    }

    enet_peer_reset(mPeer);
    spdlog::error("Connection to {}:{} failed.", address, port);
    return false;
}

void ClientThread::Disconnect()
{
    if (!mPeer || mPeer->state == ENET_PEER_STATE_DISCONNECTED)
        return;

    enet_peer_disconnect(mPeer, 0);

    ENetEvent event;
    while (enet_host_service(mHost, &event, TIMEOUT_DISCONNECT) > 0)
    {
        switch (event.type)
        {
            case ENET_EVENT_TYPE_RECEIVE:
                // Discard packets while disconnecting
                enet_packet_destroy(event.packet);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                spdlog::info("Disconnected.");
                mState = State::Idle;
                return;

            default:
                break;
        }
    }

    // Timed out waiting to disconnect. Force the connection down.
    enet_peer_reset(mPeer);
    mState = State::Idle;
}

bool ClientThread::Init()
{
    mHost = enet_host_create(nullptr, 1, 2, 0, 0);
    if (mHost == nullptr)
    {
        spdlog::error("An error occurred while trying to create an ENet client host.");
        return false;
    }

    spdlog::info("Client host created.");

    return true;
}

void ClientThread::HandleEvent(const ENetEvent& event)
{
    NetworkThread::HandleEvent(event);

    switch (event.type)
    {
        case ENET_EVENT_TYPE_DISCONNECT:
            spdlog::info("Disconnected");
            mState = State::Idle;
            break;

        case ENET_EVENT_TYPE_RECEIVE:
        {
            switch (event.channelID)
            {
                case Replication:
                {
                    {
                        std::lock_guard<std::mutex> lock(mReplicationMutex);
                        const std::vector<uint8_t> data(event.packet->data, event.packet->data + event.packet->dataLength);
                        auto request = ReplicationRequest::Deserialise(data);
                        mReplicationQueue.push(std::move(request));
                    }

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

void ClientThread::ProcessReplicationQueue(const ECS::ComponentRegistry* registry)
{
    std::queue<ReplicationRequest> queue;
    {
        std::lock_guard<std::mutex> lock(mReplicationMutex);
        queue.swap(mReplicationQueue);
    }

    const auto& ecs = registry->GetWorld();

    while (!queue.empty())
    {
        auto& req = queue.front();

        flecs::entity e;

        if (req.mIsNewEntity)
        {
            e = ecs.entity();
            mServerToClientEntities[req.mEntityId] = e.id();
        }
        else
        {
            auto it = mServerToClientEntities.find(req.mEntityId);
            if (it != mServerToClientEntities.end())
            {
                e = ecs.entity(it->second);
            }
            else
            {
                spdlog::error("Received replication request for unknown entity {}", req.mEntityId);
            }
        }

        if (req.mIsDestroyed)
        {
            e.destruct();
            mServerToClientEntities.erase(req.mEntityId);
        }
        else
        {
            for (const auto& [mTypeHash, mData] : req.mComponents)
            {
                const flecs::id_t compId = registry->GetComponentId(mTypeHash);
                if (compId == 0) continue;

                const ECS::ComponentDescriptor* descriptor = registry->TryGetDescriptor(compId);
                if (descriptor && mData.size() == descriptor->mSize)
                {
                    e.add(compId);
                    void* ptr = e.get_mut(compId);
                    memcpy(ptr, mData.data(), descriptor->mSize);
                    e.modified(compId);
                }
            }
        }

        queue.pop();
    }
}

} // namespace fc::Network
