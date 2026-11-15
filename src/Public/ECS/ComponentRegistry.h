#pragma once

#include <cstring>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <flecs.h>

#include "MessageComponents.h"
#include "ReplicatedComponent.h"
#include "Utils/String.h"

namespace fc::ECS
{

struct ComponentDescriptor
{
    flecs::id_t mComponentId{};
    size_t mSize{};
    static constexpr size_t MAX_NAME_LENGTH = 128;
    char mName[MAX_NAME_LENGTH]{};
    uint32_t mTypeHash{};
    bool mIsSingleton{false};

    ComponentDescriptor() = default;

    explicit ComponentDescriptor(const std::string& name)
    {
        strncpy(mName, name.c_str(), MAX_NAME_LENGTH - 1);
        mName[MAX_NAME_LENGTH - 1] = '\0';
    }
};

class ComponentRegistry
{
public:
    explicit ComponentRegistry(flecs::world& ecs) : mEcs(ecs) {}

    template <typename T>
    flecs::component<T> RegisterComponent()
    {
        return mEcs.component<T>();
    }

    /// @brief Registers a component as a replicated type.
    ///
    /// - In server mode, replicated components on entities that have @c fc::ECS::ReplicatedComponent are replicated to all clients.
    /// - In client and monolith modes, replicated components are treated as regular components.
    ///
    /// @note Components registered this way must be trivially copyable.
    template <typename T>
    flecs::component<T> RegisterReplicatedComponent(const std::string& name)
    {
        return RegisterReplicatedInternal<T>(name, false);
    }

    /// @brief Registers a component as a message type.
    ///
    /// - A message can be sent by creating an entity with a message component.
    /// - In server mode, the message is sent to a specific client if the message entity has @c fc::ECS::MessageRecipient, otherwise it's broadcast to all clients.
    /// - In client mode, the message is always sent to the server.
    /// - In monolith mode, messages are treated as regular components.
    /// - In all modes, message entities are cleaned up at the end of the frame.
    ///
    /// @note Components registered this way must be trivially copyable.
    template <typename T>
    flecs::component<T> RegisterMessageComponent(const std::string& name)
    {
        flecs::component<T> type = this->RegisterComponent<T>();

        ComponentDescriptor desc(name);
        desc.mComponentId = mEcs.id<T>();
        desc.mSize = sizeof(T);
        desc.mTypeHash = Utils::String::HashString(name);
        desc.mIsSingleton = false;

        mMessageIdToDescriptor[desc.mComponentId] = desc;
        mMessageHashToId[desc.mTypeHash] = desc.mComponentId;
        mMessageComponents.insert(desc.mComponentId);

        mEcs.observer()
            .with<T>()
            .event(flecs::OnAdd)
            .each([](const flecs::entity e) {
                if (!e.has<MessageIncomingTag>())
                {
                    e.add<MessageOutgoingTag>();
                }
            });

        return type;
    }

    template <typename T>
    flecs::component<T> RegisterReplicatedSingleton(const std::string& name)
    {
        return RegisterReplicatedInternal<T>(name, true);
    }

    const std::unordered_set<flecs::id_t>& GetReplicatedComponents() const
    {
        return mComponents;
    }

    /// @brief Looks up a component descriptor.
    /// @return A pointer to the descriptor, or nullptr if the id isn't a registered replicated component.
    const ComponentDescriptor* TryGetDescriptor(const flecs::id_t componentId) const
    {
        const auto it = mIdToDescriptor.find(componentId);
        return it != mIdToDescriptor.end() ? &it->second : nullptr;
    }

    flecs::id_t GetComponentId(const uint32_t typeHash) const
    {
        auto it = mHashToId.find(typeHash);
        if (it != mHashToId.end())
        {
            return it->second;
        }
        return 0;
    }

    /// @brief Looks up a message component descriptor.
    /// @return A pointer to the descriptor, or nullptr if the id isn't a registered message component.
    const ComponentDescriptor* TryGetMessageDescriptor(const flecs::id_t componentId) const
    {
        const auto it = mMessageIdToDescriptor.find(componentId);
        return it != mMessageIdToDescriptor.end() ? &it->second : nullptr;
    }

    /// @brief Resolves a serialised message type hash to a component id.
    /// @return The component id, or 0 if the hash isn't a registered message component.
    flecs::id_t GetMessageComponentId(const uint32_t typeHash) const
    {
        auto it = mMessageHashToId.find(typeHash);
        if (it != mMessageHashToId.end())
        {
            return it->second;
        }
        return 0;
    }

    /// @brief All registered message component ids.
    const std::unordered_set<flecs::id_t>& GetMessageComponents() const
    {
        return mMessageComponents;
    }

    flecs::world& GetWorld() const
    {
        return mEcs;
    }

private:
    flecs::world& mEcs;

    std::unordered_map<flecs::id_t, ComponentDescriptor> mIdToDescriptor;
    std::unordered_map<uint32_t, flecs::id_t> mHashToId;
    std::unordered_set<flecs::id_t> mComponents;

    std::unordered_map<flecs::id_t, ComponentDescriptor> mMessageIdToDescriptor;
    std::unordered_map<uint32_t, flecs::id_t> mMessageHashToId;
    std::unordered_set<flecs::id_t> mMessageComponents;

    std::queue<uint64_t> mDestructionQueue;

    void InitEntityObservers()
    {
        mEcs.observer<ReplicatedComponent>()
            .event(flecs::OnAdd)
            .each([](ReplicatedComponent& rep) {
                rep.mIsDirty = true;
                rep.mIsNewEntity = true;
            });

        mEcs.observer()
            .with<ReplicatedComponent>()
            .event(flecs::OnRemove)
            .each([this](const flecs::entity e) {
                mDestructionQueue.push(e.id());
            });
    }

    template <typename T>
    void InitComponentObserver()
    {
        mEcs.observer<ReplicatedComponent>()
            .with<T>()
            .event(flecs::OnAdd)
            .each([](const flecs::entity e, ReplicatedComponent& rep) {
                // Don't mark as dirty if the entity is new - that case is handled by the
                // entity OnAdd observer (see InitEntityObservers above)
                if (!rep.mIsNewEntity)
                {
                    rep.MarkDirty(e.world().id<T>());
                }
            });

        mEcs.observer<ReplicatedComponent>()
            .with<T>()
            .event(flecs::OnSet)
            .each([](const flecs::entity e, ReplicatedComponent& rep) {
                rep.MarkDirty(e.world().id<T>());
            });

        mEcs.observer<ReplicatedComponent>()
            .with<T>()
            .event(flecs::OnRemove)
            .each([](ReplicatedComponent& rep) {
                // Mark the entity as dirty so the removal can be replicated without
                // sending the component data
                rep.mIsDirty = true;
            });
    }

    template <typename T>
    flecs::component<T> RegisterReplicatedInternal(const std::string& name, bool isSingleton)
    {
        flecs::component<T> type = this->RegisterComponent<T>();

        ComponentDescriptor desc(name);
        desc.mComponentId = mEcs.id<T>();
        desc.mSize = sizeof(T);
        desc.mTypeHash = Utils::String::HashString(name);
        desc.mIsSingleton = isSingleton;

        mIdToDescriptor[desc.mComponentId] = desc;
        mHashToId[desc.mTypeHash] = desc.mComponentId;
        mComponents.insert(desc.mComponentId);

        this->InitComponentObserver<T>();

        if (isSingleton)
        {
            type.add(flecs::Singleton);

            flecs::id id = mEcs.id<T>();
            flecs::entity e = id.entity();
            e.add<ReplicatedComponent>();
        }

        return type;
    }
};

} // namespace fc::ECS
