#pragma once

#include <flecs.h>

#include "Core/Core.h"

namespace fc
{

CORE_API extern flecs::entity PreDraw;
CORE_API extern flecs::entity Draw3D;
CORE_API extern flecs::entity Draw2D;
CORE_API extern flecs::entity PostDraw;

void InitPhases(const flecs::world& ecs);

}; // namespace fc

