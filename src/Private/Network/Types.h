#pragma once

#include <cstdint>
#include <vector>

#include <enet/enet.h>

namespace fc::Network
{

/// @brief Represents a generic message ready to send to a peer.
struct OutboundMessage
{
    std::vector<uint8_t> mData;
    ENetPeer* mPeer;
    uint32_t mChannel{0};
    uint32_t mFlags{ENET_PACKET_FLAG_RELIABLE};
};

}; // namespace fc::Network
