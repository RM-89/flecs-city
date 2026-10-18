#include "Editor.h"

namespace fc::Editor
{

static void RegisterAssetTypes()
{
}

static void RegisterComponents(ECS::ComponentRegistry* registry)
{
}

static void InitCommonECS(const flecs::world& ecs)
{
}

static void InitServerECS(const flecs::world& ecs)
{
}

static void InitClientECS(const flecs::world& ecs)
{
}

static void Cleanup(const flecs::world& ecs)
{
}

Module MODULE{
    .RegisterAssetTypes = &RegisterAssetTypes,
    .RegisterComponents = &RegisterComponents,
    .InitCommonECS = &InitCommonECS,
    .InitServerECS = &InitServerECS,
    .InitClientECS = &InitClientECS,
    .Cleanup = &Cleanup
};

}; // namespace fc::Editor
