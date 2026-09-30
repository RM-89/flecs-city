#pragma once

#include <cstdint>
#include <vector>
#include <unordered_set>

#include <enet/enet.h>

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

protected:
    bool Init() override;
    void HandleEvent(const ENetEvent& event) override;
    void Update() override;

private:
    /// @brief All connected peers
    std::vector<ENetPeer*> mConnectedPeers;

    /// @brief Peers that have connected and need to be sent a full snapshot
    std::unordered_set<ENetPeer*> mNewPeers;

    /// @brief Peers that have received a full snapshot and are ready to receive deltas
    std::unordered_set<ENetPeer*> mReadyPeers;

    std::mutex mReplicationMutex;
    std::queue<ReplicationRequest> mReplicationQueue;

    void ProcessReplicationQueue();
};

} // namespace fc::Network
