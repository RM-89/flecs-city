#pragma once

#include <cstdint>

namespace fc
{

/// @brief Message sent by the client to request a pong from the server.
/// The timestamp is echoed back in @c PongMessage so the client can measure round trip time.
struct PingMessage
{
    uint64_t mTimestamp{0};
};

}; // namespace fc
