#pragma once

#include <cstdint>

#include <enet/enet.h>

#include <queue>
#include <mutex>
#include <unordered_map>

#include "Network/NetworkThread.h"
#include "Network/ReplicationRequest.h"

namespace fc::ECS
{
class ComponentRegistry;
}

namespace fc::Network
{

constexpr int TIMEOUT_CONNECT = 5000;
constexpr int TIMEOUT_DISCONNECT = 3000;

class ClientThread : public NetworkThread
{
public:
    bool Connect(const char* address, uint32_t port);
    void Disconnect() override;

    /// @brief Queues a message for sending to the server on the Messaging channel.
    void QueueECSMessage(const ECS::Message& message) override;

    void ProcessReplicationQueue(const fc::ECS::ComponentRegistry* registry);

protected:
    bool Init() override;
    void HandleEvent(const ENetEvent& event) override;

private:
    ENetPeer* mPeer{nullptr};

    std::mutex mReplicationMutex;
    std::queue<ReplicationRequest> mReplicationQueue;
    std::unordered_map<uint64_t, flecs::entity_t> mServerToClientEntities;
};

} // namespace fc::Network
