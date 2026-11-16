#include "ServerThread.h"

#include <algorithm>

#include <enet/enet.h>
#include <spdlog/spdlog.h>

#include "Constants.h"
#include "ECS/Message.h"
#include "Network/ReplicationRequest.h"

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

void ServerThread::QueueECSMessage(const ECS::Message& message)
{
    if (message.mRecipient != nullptr)
    {
        QueueECSMessageTo(message, message.mRecipient);
        return;
    }

    // This is a broadcast - snapshot the peers so we don't hold the lock while queueing
    std::vector<ENetPeer*> peers;
    {
        std::lock_guard<std::mutex> lock(mConnectedPeersMutex);
        peers = mConnectedPeers;
    }

    for (ENetPeer* peer : peers)
    {
        QueueECSMessageTo(message, peer);
    }
}

std::vector<ENetPeer*> ServerThread::PopNewPeers()
{
    std::lock_guard<std::mutex> lock(mReplicationMutex);
    std::vector<ENetPeer*> peers(mNewPeers.begin(), mNewPeers.end());
    mNewPeers.clear();
    return peers;
}

bool ServerThread::Init()
{
    mConnectedPeers.clear();
    mNewPeers.clear();
    mReadyPeers.clear();

    mHost = enet_host_create(&mAddress, MAX_CLIENTS, CHANNEL_COUNT, 0, 0);
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

                size_t totalClients;
                {
                    std::lock_guard<std::mutex> lock(mConnectedPeersMutex);
                    mConnectedPeers.push_back(event.peer);
                    totalClients = mConnectedPeers.size();
                }

                {
                    std::lock_guard<std::mutex> lock(mReplicationMutex);
                    mNewPeers.insert(event.peer);
                }

                spdlog::info("Client connected from {}:{} (peer ID: {}). Total clients: {}.", hostStr, event.peer->address.port, event.peer->incomingPeerID, totalClients);
            }
            break;

        case ENET_EVENT_TYPE_DISCONNECT:
            {
                char hostStr[64];
                enet_address_get_host_ip(&event.peer->address, hostStr, sizeof(hostStr));

                size_t totalClients;
                {
                    std::lock_guard<std::mutex> lock(mConnectedPeersMutex);
                    auto it = std::find(mConnectedPeers.begin(), mConnectedPeers.end(), event.peer);
                    if (it != mConnectedPeers.end())
                    {
                        mConnectedPeers.erase(it);
                    }
                    totalClients = mConnectedPeers.size();
                }

                {
                    std::lock_guard<std::mutex> lock(mReplicationMutex);
                    mNewPeers.erase(event.peer);
                    mReadyPeers.erase(event.peer);
                }

                spdlog::info("Client disconnected from {}:{} (peer ID: {}). Remaining clients: {}.", hostStr, event.peer->address.port, event.peer->incomingPeerID, totalClients);
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
            QueueMessageTo(buffer, packet.mRecipient, Channel::Replication, ENET_PACKET_FLAG_UNSEQUENCED);
            mReadyPeers.insert(packet.mRecipient);
        }
        else
        {
            // This is a broadcast - send to ready peers only
            for (ENetPeer* peer : mReadyPeers)
            {
                QueueMessageTo(buffer, peer, Channel::Replication, ENET_PACKET_FLAG_UNSEQUENCED);
            }
        }

        mReplicationQueue.pop();
    }
}

} // namespace fc::Network
