#pragma once

#include <flecs.h>

#include "ECS/ComponentRegistry.h"

namespace fc
{

/// @brief Function pointers for initialising a module.
struct Module
{
    /// @brief For registering asset types via fc::Assets::RegisterType.
    void (*RegisterAssetTypes)();

    /// @brief For registering ECS components. Called for all modules before the Init* functions below.
    /// @param registry The component registry to use.
    void (*RegisterComponents)(ECS::ComponentRegistry* registry);

    /// @brief For initialising common ECS entities (including systems/queries).
    /// @param ecs The flecs world.
    void (*InitCommonECS)(flecs::world& ecs);

    /// @brief For initialising server-side ECS entities (including systems/queries).
    /// @param ecs The flecs world.
    void (*InitServerECS)(flecs::world& ecs);

    /// @brief For initialising client-side ECS entities (including systems/queries).
    /// @param ecs The flecs world.
    void (*InitClientECS)(flecs::world& ecs);

    /// @brief For carrying out any necessary module cleanup when shutting down.
    /// @param ecs The flecs world.
    void (*Cleanup)(flecs::world& ecs);
};

}; // namespace fc
