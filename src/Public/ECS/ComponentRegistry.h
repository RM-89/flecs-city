#pragma once

#include <cstring>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <flecs.h>

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

    template <typename T>
    flecs::component<T> RegisterReplicatedComponent(const std::string& name)
    {
        flecs::component<T> type = this->RegisterComponent<T>();

        ComponentDescriptor desc(name);
        desc.mComponentId = mEcs.id<T>();
        desc.mSize = sizeof(T);
        desc.mTypeHash = Utils::String::HashString(name);

        mIdToDescriptor[desc.mComponentId] = desc;
        mHashToId[desc.mTypeHash] = desc.mComponentId;
        mComponents.insert(desc.mComponentId);

        this->InitComponentObserver<T>();

        return type;
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

    flecs::world& GetWorld() const
    {
        return mEcs;
    }

private:
    flecs::world& mEcs;

    std::unordered_map<flecs::id_t, ComponentDescriptor> mIdToDescriptor;
    std::unordered_map<uint32_t, flecs::id_t> mHashToId;
    std::unordered_set<flecs::id_t> mComponents;

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
};

} // namespace fc::ECS
