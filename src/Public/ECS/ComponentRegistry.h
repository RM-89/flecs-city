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
    flecs::id_t mComponentId;
    size_t mSize;
    static constexpr size_t MAX_NAME_LENGTH = 128;
    char mName[MAX_NAME_LENGTH];
    uint32_t mTypeHash;

    ComponentDescriptor() = default;
    ComponentDescriptor(const std::string& name)
    {
        strncpy(mName, name.c_str(), MAX_NAME_LENGTH - 1);
        mName[MAX_NAME_LENGTH - 1] = '\0';
    }
};

class ComponentRegistry
{
   public:
    ComponentRegistry(flecs::world& ecs) : mEcs(ecs) {}

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

    const ComponentDescriptor& GetDescriptor(const flecs::id_t componentId) const
    {
        return mIdToDescriptor.at(componentId);
    }

    flecs::id_t GetComponentId(uint32_t typeHash) const
    {
        auto it = mHashToId.find(typeHash);
        if (it != mHashToId.end())
        {
            return it->second;
        }
        return 0;  // Return 0 or handle error if not found
    }

    flecs::world& GetWorld() { return mEcs; }

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
            .each([](flecs::entity e, ReplicatedComponent& rep) {
                rep.mIsDirty = true;
                rep.mIsNewEntity = true;
            });

        mEcs.observer<ReplicatedComponent>()
            .event(flecs::OnRemove)
            .each([this](flecs::entity e, ReplicatedComponent& rep) {
                mDestructionQueue.push(e.id());
            });
    }

    template <typename T>
    void InitComponentObserver()
    {
        mEcs.observer<T, ReplicatedComponent>()
            .event(flecs::OnAdd)
            .each([](flecs::entity e, T& component, ReplicatedComponent& rep) {
                // Don't mark as dirty if the entity is new - that case is handled by the
                // entity OnAdd observer (see InitEntityObservers above)
                if (!rep.mIsNewEntity)
                {
                    rep.MarkDirty(e.world().id<T>());
                }
            });

        mEcs.observer<T, ReplicatedComponent>()
            .event(flecs::OnSet)
            .each([](flecs::entity e, T& component, ReplicatedComponent& rep) {
                rep.MarkDirty(e.world().id<T>());
            });

        mEcs.observer<T, ReplicatedComponent>()
            .event(flecs::OnRemove)
            .each([](flecs::entity e, T& component, ReplicatedComponent& rep) {
                // Mark the entity as dirty so the removal can be replicated without
                // sending the component data
                rep.mIsDirty = true;
            });
    }
};

}  // namespace fc::ECS
