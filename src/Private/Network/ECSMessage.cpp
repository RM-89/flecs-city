#pragma once

#include <flecs.h>
#include <spdlog/spdlog.h>

#include "ECSMessage.h"
#include "ECS/Message.h"

namespace fc::Network
{

/// @brief Creates an entity for the given message.
/// @return The entity, or a null entity if the message couldn't be applied.
static flecs::entity CreateEntity(const ECS::ComponentRegistry& registry, const ECS::Message& message)
{
    const flecs::id_t componentId = registry.GetMessageComponentId(message.mTypeHash);
    if (componentId == 0)
    {
        spdlog::warn("Received a message with unknown type hash {}; dropping it.", message.mTypeHash);
        return flecs::entity::null();
    }

    const ECS::ComponentDescriptor* desc = registry.TryGetMessageDescriptor(componentId);
    if (desc == nullptr)
    {
        return flecs::entity::null();
    }

    if (message.mData.size() != desc->mSize)
    {
        spdlog::warn("Received a '{}' message of {} bytes but expected {}; dropping it.",
            desc->mName, message.mData.size(), desc->mSize);
        return flecs::entity::null();
    }

    flecs::world& ecs = registry.GetWorld();
    flecs::entity entity = ecs.entity();

    // Tag before adding the component so the registration observer doesn't mark
    // the entity as outgoing (which would echo it straight back to the sender)
    entity.add<ECS::MessageIncomingTag>();
    entity.add(componentId);
    memcpy(entity.get_mut(componentId), message.mData.data(), desc->mSize);
    entity.modified(componentId);
    entity.set<ECS::MessageSender>({message.mSender});

    return entity;
}

void ProcessStartFrameECSMessages(NetworkThread* thread, const ECS::ComponentRegistry& registry)
{
    flecs::world& ecs = registry.GetWorld();

    auto processedIncoming = ecs.query_builder<>()
        .with<ECS::MessageIncomingTag>()
        .with<ECS::MessageProcessedTag>()
        .build();

    ecs.defer_begin();
    processedIncoming.each([](const flecs::entity entity) {
        entity.destruct();
    });
    ecs.defer_end();


    if (thread != nullptr)
    {
        ECS::Message message;
        while (thread->TryPopECSMessage(message))
        {
            CreateEntity(registry, message);
        }
    }
}

void ProcessEndFrameECSMessages(NetworkThread* thread, const ECS::ComponentRegistry& registry)
{
    flecs::world& ecs = registry.GetWorld();

    // 1. Clean up processed outgoing messages from the previous tick
    auto processedOutgoing = ecs.query_builder<>()
        .with<ECS::MessageOutgoingTag>()
        .with<ECS::MessageProcessedTag>()
        .build();

    ecs.defer_begin();
    processedOutgoing.each([](const flecs::entity entity) {
        entity.destruct();
    });
    ecs.defer_end();

    // 2. Send outgoing message entities, then collect them for destruction
    auto outgoing = ecs.query_builder()
        .with<ECS::MessageOutgoingTag>()
        .without<ECS::MessageIncomingTag>()
        .without<ECS::MessageProcessedTag>()
        .build();

    outgoing.each([&](const flecs::entity entity) {
        const ECS::MessageRecipient* recipient = entity.try_get<ECS::MessageRecipient>();

        bool isMessage = false;
        for (const flecs::id_t componentId : registry.GetMessageComponents())
        {
            if (!entity.has(componentId)) continue;

            const ECS::ComponentDescriptor* desc = registry.TryGetMessageDescriptor(componentId);
            const auto* data = static_cast<const uint8_t*>(entity.try_get(componentId));
            if (desc == nullptr || data == nullptr) continue;

            isMessage = true;

            if (thread != nullptr)
            {
                ECS::Message message;
                message.mTypeHash = desc->mTypeHash;
                message.mData.assign(data, data + desc->mSize);
                message.mRecipient = recipient != nullptr ? recipient->mPeer : nullptr;

                thread->QueueECSMessage(message);
            }
            else
            {
                // No network thread; this message is for the local ECS world.
                entity.set<ECS::MessageSender>({ nullptr });
            }
        }

        if (!isMessage)
        {
            spdlog::warn("Entity {} is tagged as a message but has no registered message component.", entity.id());
        }

        entity.add<ECS::MessageProcessedTag>();
    });

    // 3. Mark received message entities as processed
    auto incoming = ecs.query_builder()
        .with<ECS::MessageIncomingTag>()
        .build();

    incoming.each([&](const flecs::entity entity) {
        entity.add<ECS::MessageProcessedTag>();
    });
}

} // namespace fc::Network