#include "Phases.h"

namespace fc
{

flecs::entity PreDraw;
flecs::entity Draw3D;
flecs::entity Draw2D;
flecs::entity DrawUI;
flecs::entity PostDraw;

void InitPhases(const flecs::world& ecs)
{
    PreDraw = ecs.entity("fc::PreDraw").add(flecs::Phase).depends_on(flecs::OnUpdate);
    Draw3D = ecs.entity("fc::Draw3D").add(flecs::Phase).depends_on(PreDraw);
    Draw2D = ecs.entity("fc::Draw2D").add(flecs::Phase).depends_on(Draw3D);
    DrawUI = ecs.entity("fc::DrawUI").add(flecs::Phase).depends_on(Draw2D);
    PostDraw = ecs.entity("fc::PostDraw").add(flecs::Phase).depends_on(DrawUI);
}

}; // namespace fc

