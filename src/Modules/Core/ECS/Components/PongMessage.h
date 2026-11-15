#pragma once

#include <cstdint>

namespace fc::Core
{

/// @brief Message sent by the server in response to a @c PingMessage, echoing its timestamp.
struct PongMessage
{
    uint64_t mTimestamp{0};
};

} // namespace fc::Core
