#pragma once

#include "NetworkThread.h"
#include "ECS/ComponentRegistry.h"

namespace fc::Network
{

/// @brief Deletes processed entities from messages that were received in the previous tick and creates entities from
/// incoming message packets.
///
/// @param thread The network thread to process messages from, or nullptr if the messages are local (monolith mode).
/// @param registry The component registry to resolve components from.
void ProcessStartFrameECSMessages(NetworkThread* thread, const ECS::ComponentRegistry& registry);

/// @brief Deletes processed entities from messages that were sent in the previous tick, sends new messages created in
/// this frame, and marks received message entities as processed ready for cleanup by @c ProcessStartFrameECSMessages
/// in the next tick.
///
/// @param thread The network thread to send with, or nullptr if the messages are local (monolith mode).
/// @param registry The component registry to resolve components from.
void ProcessEndFrameECSMessages(NetworkThread* thread, const ECS::ComponentRegistry& registry);

} // namespace fc::Network
