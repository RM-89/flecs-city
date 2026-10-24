#include "ClientThread.h"

#include <cstdlib>

#include <enet/enet.h>
#include <spdlog/spdlog.h>

#include "ECS/ComponentRegistry.h"
#include "Network/ReplicationRequest.h"

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

                        if (request.mType > ReplicationType::Destroyed)
                        {
                            spdlog::warn("Received invalid replication request type {}; skipping", static_cast<uint8_t>(request.mType));
                        }
                        else
                        {
                            mReplicationQueue.push(std::move(request));
                        }
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

    auto LookupEntity = [&](const uint64_t entityId) {
        auto it = mServerToClientEntities.find(entityId);
        if (it != mServerToClientEntities.end())
        {
            return ecs.entity(it->second);
        }

        spdlog::error("Received replication request for unknown entity {}", entityId);
        return flecs::entity::null();
    };

    auto ApplyComponent = [](const flecs::entity& target, const flecs::id_t compId,
                         const ECS::ComponentDescriptor& desc,
                         const std::vector<uint8_t>& data)
    {
        target.add(compId);
        memcpy(target.get_mut(compId), data.data(), desc.mSize);
        target.modified(compId);
    };

    auto UpdateEntity = [&](const flecs::entity& e, const ReplicationRequest& req) {
        for (const auto& [typeHash, data] : req.mComponents)
        {
            const flecs::id_t compId = registry->GetComponentId(typeHash);
            if (compId == 0) continue;
            const auto* desc = registry->TryGetDescriptor(compId);
            if (!desc || data.size() != desc->mSize) continue;
            ApplyComponent(e, compId, *desc, data);
        }
    };

    while (!queue.empty())
    {
        auto& req = queue.front();

        switch (req.mType)
        {
            case ReplicationType::Update:
                {
                    auto e = LookupEntity(req.mEntityId);
                    if (e) UpdateEntity(e, req);
                }
                break;
            case ReplicationType::NewEntity:
                {
                    auto e = ecs.entity();
                    mServerToClientEntities[req.mEntityId] = e.id();

                    UpdateEntity(e, req);
                }
                break;
            case ReplicationType::Singleton:
                for (const auto& [typeHash, data] : req.mComponents)
                {
                    const flecs::id_t compId = registry->GetComponentId(typeHash);
                    if (compId == 0) continue; // component not registered on this client

                    const auto* desc = registry->TryGetDescriptor(compId);
                    if (!desc || data.size() != desc->mSize) continue;
                    if (!desc->mIsSingleton)
                    {
                        spdlog::warn("Received singleton replication for non-singleton component {}", desc->mName);
                        continue;
                    }

                    ApplyComponent(ecs.entity(compId), compId, *desc, data);
                }
                break;
            case ReplicationType::Destroyed:
                if (const auto e = LookupEntity(req.mEntityId))
                {
                    e.destruct();
                    mServerToClientEntities.erase(req.mEntityId);
                }
                break;
        }

        queue.pop();
    }
}

} // namespace fc::Network
