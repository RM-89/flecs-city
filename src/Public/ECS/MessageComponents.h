#pragma once

#include <enet/enet.h>

namespace fc::ECS
{

struct MessageOutgoingTag {};
struct MessageIncomingTag {};

struct MessageRecipient
{
    ENetPeer* mPeer{nullptr};
};

struct MessageSender
{
    ENetPeer* mPeer{nullptr};
};

} // namespace fc::ECS
