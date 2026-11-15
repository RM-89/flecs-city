#pragma once

#include <cstdint>
#include <vector>
#include <unordered_set>

#include <enet/enet.h>

#include "ECS/Message.h"
#include "Network/NetworkThread.h"
#include "Network/ReplicationRequest.h"

namespace fc::Network
{

class ServerThread : public NetworkThread
{
public:
    explicit ServerThread(uint32_t listenPort);

    std::vector<ENetPeer*> PopNewPeers();
    void SetClientReady(ENetPeer* peer);

    void QueueReplicationRequest(const ReplicationRequest& packet);

    /// @brief Queues a message for sending on the Messaging channel.
    /// Sends to @c ECS::Message::mRecipient if set, otherwise broadcasts to all connected peers.
    void QueueECSMessage(const ECS::Message& message) override;

protected:
    bool Init() override;
    void HandleEvent(const ENetEvent& event) override;
    void Update() override;

private:
    /// @brief All connected peers
    std::vector<ENetPeer*> mConnectedPeers;
    std::mutex mConnectedPeersMutex;

    /// @brief Peers that have connected and need to be sent a full snapshot
    std::unordered_set<ENetPeer*> mNewPeers;

    /// @brief Peers that have received a full snapshot and are ready to receive deltas
    std::unordered_set<ENetPeer*> mReadyPeers;

    std::queue<ReplicationRequest> mReplicationQueue;
    std::mutex mReplicationMutex;

    void ProcessReplicationQueue();
};

} // namespace fc::Network
