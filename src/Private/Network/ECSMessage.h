#pragma once

#include "NetworkThread.h"
#include "ECS/ComponentRegistry.h"

namespace fc::Network
{

/// @brief Creates entities from incoming message packets.
///
/// @param thread The network thread to process messages from.
/// @param registry The component registry to resolve components from.
void DrainIncomingECSMessages(NetworkThread& thread, const ECS::ComponentRegistry& registry);

/// @brief Sends outgoing message entities and destroys all processed message entities.
///
/// @param thread The network thread to send with, or nullptr to drop outgoing messages instead (used in monolith mode, which has no peers).
/// @param registry The component registry to resolve components from.
void ProcessEndFrameECSMessages(NetworkThread* thread, const ECS::ComponentRegistry& registry);

} // namespace fc::Network
