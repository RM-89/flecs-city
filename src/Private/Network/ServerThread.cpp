#include "ServerThread.h"

#include <algorithm>
#include <cstdint>
#include "Network/ReplicationRequest.h"

#include <enet/enet.h>
#include <spdlog/spdlog.h>

namespace fc::Network
{

ServerThread::ServerThread(const uint32_t listenPort)
    : NetworkThread()
{
    mAddress.host = ENET_HOST_ANY;
    mAddress.port = listenPort;
}

void ServerThread::QueueReplicationRequest(const ReplicationRequest& packet)
{
    std::lock_guard<std::mutex> lock(mReplicationMutex);
    mReplicationQueue.push(packet);
}

std::vector<ENetPeer*> ServerThread::PopNewPeers()
{
    std::lock_guard<std::mutex> lock(mReplicationMutex);
    std::vector<ENetPeer*> peers(mNewPeers.begin(), mNewPeers.end());
    mNewPeers.clear();
    return peers;
}

void ServerThread::SetClientReady(ENetPeer* peer)
{
    std::lock_guard<std::mutex> lock(mReplicationMutex);
    mReadyPeers.insert(peer); 
}

bool ServerThread::Init()
{
    mConnectedPeers.clear();
    mNewPeers.clear();
    mReadyPeers.clear();

    mHost = enet_host_create(&mAddress, 32, 2, 0, 0);
    if (mHost == NULL)
    {
        spdlog::error("An error occurred while trying to create an ENet server host.");
        return false;
    }

    spdlog::info("Server host created.");
    mState = State::Active;

    return true;
}

void ServerThread::HandleEvent(const ENetEvent& event)
{
    NetworkThread::HandleEvent(event);

    switch (event.type)
    {
        case ENET_EVENT_TYPE_CONNECT:
            {
                char hostStr[64];
                enet_address_get_host_ip(&event.peer->address, hostStr, sizeof(hostStr));

                mConnectedPeers.push_back(event.peer);
                
                {
                    std::lock_guard<std::mutex> lock(mReplicationMutex);
                    mNewPeers.insert(event.peer);
                }

                spdlog::info("Client connected from {}:{} (peer ID: {}). Total clients: {}.", hostStr, event.peer->address.port, event.peer->incomingPeerID, mConnectedPeers.size());

                QueueMessage("Welcome", event.peer, Channel::General, ENET_PACKET_FLAG_RELIABLE);
            }
            break;

        case ENET_EVENT_TYPE_DISCONNECT:
            {
                char hostStr[64];
                enet_address_get_host_ip(&event.peer->address, hostStr, sizeof(hostStr));

                auto it = std::find(mConnectedPeers.begin(), mConnectedPeers.end(), event.peer);
                if (it != mConnectedPeers.end())
                {
                    mConnectedPeers.erase(it);
                }

                {
                    std::lock_guard<std::mutex> lock(mReplicationMutex);
                    mNewPeers.erase(event.peer);
                    mReadyPeers.erase(event.peer);
                }

                spdlog::info("Client disconnected from {}:{} (peer ID: {}). Remaining clients: {}.", hostStr, event.peer->address.port, event.peer->incomingPeerID, mConnectedPeers.size());
            }
            break;

        default:
            break;
    }
}

void ServerThread::Update()
{
    ProcessReplicationQueue();
}

void ServerThread::ProcessReplicationQueue()
{
    std::lock_guard<std::mutex> lock(mReplicationMutex);

    while (!mReplicationQueue.empty())
    {
        ReplicationRequest& packet = mReplicationQueue.front();

        std::vector<uint8_t> buffer = packet.Serialise();

        if (packet.mRecipient != nullptr)
        {
            QueueMessage(buffer, packet.mRecipient, Channel::Replication, ENET_PACKET_FLAG_UNSEQUENCED);
            mReadyPeers.insert(packet.mRecipient);
        }
        else
        {
            // This is a broadcast - send to ready peers only
            for (ENetPeer* peer : mReadyPeers)
            {
                QueueMessage(buffer, peer, Channel::Replication, ENET_PACKET_FLAG_UNSEQUENCED);
            }
        }

        mReplicationQueue.pop();
    }
}

} // namespace fc::Network
