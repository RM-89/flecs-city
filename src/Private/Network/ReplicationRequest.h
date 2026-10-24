#pragma once

#include <vector>

#include <enet/enet.h>
#include <flecs.h>

#include "ECS/ReplicatedComponent.h"
#include "ECS/ComponentRegistry.h"

namespace fc::Network
{

enum class ReplicationType : uint8_t
{
    Update = 0,
    NewEntity = 1,
    Singleton = 2,
    Destroyed = 3,
};

/// @brief Represents a replication request for a single entity.
struct ReplicationRequest
{
    struct ComponentData
    {
        uint32_t mTypeHash{};
        std::vector<uint8_t> mData;
    };

    // Serialised fields
    uint64_t mEntityId{};
    ReplicationType mType{};
    std::vector<ComponentData> mComponents;

    ENetPeer* mRecipient = nullptr;

    std::vector<uint8_t> Serialise()
    {
        std::vector<uint8_t> buffer;
        buffer.reserve(256);

        auto Write = [&buffer](const void* data, const size_t size) {
            const auto* bytes = static_cast<const uint8_t*>(data);
            buffer.insert(buffer.end(), bytes, bytes + size);
        };

        Write(&mEntityId, sizeof(mEntityId));
        Write(&mType, sizeof(mType));

        const auto componentCount = static_cast<uint16_t>(mComponents.size());
        Write(&componentCount, sizeof(componentCount));

        for (const auto& [mTypeHash, mData] : mComponents)
        {
            Write(&mTypeHash, sizeof(mTypeHash));

            auto dataSize = static_cast<uint16_t>(mData.size());
            Write(&dataSize, sizeof(dataSize));

            buffer.insert(buffer.end(), mData.begin(), mData.end());
        }

        return buffer;
    }

    static ReplicationRequest Deserialise(const std::vector<uint8_t>& buffer)
    {
        ReplicationRequest request;
        size_t offset = 0;

        auto Read = [&buffer, &offset](void* dest, const size_t size) {
            memcpy(dest, buffer.data() + offset, size);
            offset += size;
        };

        Read(&request.mEntityId, sizeof(request.mEntityId));
        Read(&request.mType, sizeof(request.mType));

        uint16_t componentCount;
        Read(&componentCount, sizeof(componentCount));

        for (uint16_t i = 0; i < componentCount; ++i)
        {
            ComponentData comp;

            Read(&comp.mTypeHash, sizeof(comp.mTypeHash));

            uint16_t dataSize;
            Read(&dataSize, sizeof(dataSize));

            comp.mData.resize(dataSize);
            memcpy(comp.mData.data(), buffer.data() + offset, dataSize);
            offset += dataSize;

            request.mComponents.push_back(std::move(comp));
        }

        return request;
    }
};

/// @brief Generates a replication request for the given entity.
///
/// @param entity The entity being replicated.
/// @param rep The entity's @c ReplicatedComponent.
/// @param registry The @c ComponentRegistry to retrieve component descriptors from.
/// @param forceNew Whether to mark the request as a new entity replication even if @c ReplicatedComponent is not marked as such. Useful for updating newly connected clients.
/// @return The request.
[[nodiscard]] inline ReplicationRequest GenerateReplicationRequest(const flecs::entity entity, const ReplicatedComponent& rep, const ECS::ComponentRegistry& registry, const bool forceNew)
{
    ReplicationRequest request;
    request.mEntityId = entity.id();

    const bool isFullState = forceNew || rep.mIsNewEntity;

    if (const auto* srcDesc = registry.TryGetDescriptor(entity.id()); srcDesc && srcDesc->mIsSingleton)
    {
        request.mType = ReplicationType::Singleton;
    }
    // TODO: handle destroyed entities
    else
    {
        request.mType = isFullState ? ReplicationType::NewEntity : ReplicationType::Update;
    }

    auto addComponent = [&](const flecs::id_t componentId)
    {
        if (const void* componentData = entity.try_get(componentId))
        {
            const auto* desc = registry.TryGetDescriptor(componentId);
            if (!desc) return;
            ReplicationRequest::ComponentData compData;
            compData.mTypeHash = desc->mTypeHash;
            const auto* dataPtr = static_cast<const uint8_t*>(componentData);
            compData.mData.assign(dataPtr, dataPtr + desc->mSize);
            request.mComponents.push_back(std::move(compData));
        }
    };

    if (isFullState)
    {
        for (const flecs::id_t componentId : registry.GetReplicatedComponents())
        {
            addComponent(componentId);
        }
    }
    else
    {
        for (size_t i = 0; i < rep.mDirtyComponentCount; ++i)
        {
            addComponent(rep.mDirtyComponents[i]);
        }
    }

    return request;
}

} // namespace fc::Network
