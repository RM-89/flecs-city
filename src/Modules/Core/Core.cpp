#include "Core.h"

#include <strings.h>

#include <flecs.h>
#include <raylib.h>
#include <chrono>
#include <spdlog/spdlog.h>

#include "Assets/Assets.h"
#include "Assets/ModelAsset.h"

#include "ECS/ComponentRegistry.h"
#include "ECS/ReplicatedComponent.h"

#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/ModelComponent.h"
#include "ECS/Components/ModelInstanceComponent.h"
#include "ECS/Components/PositionComponent.h"
#include "ECS/Components/TextComponent.h"
#include "ECS/Phases.h"

namespace fc::Core
{

static flecs::system gPreDrawSystem;
static flecs::system gEndDrawSystem;

static void RegisterAssetTypes()
{
    Assets::RegisterType(ModelAsset::Type, &ModelAsset::Deserialise);
}

static void RegisterComponents(ECS::ComponentRegistry* registry)
{
    registry->RegisterComponent<ReplicatedComponent>();

    registry->RegisterComponent<CameraComponent>().add(flecs::Singleton);
    registry->RegisterComponent<ModelInstanceComponent>();

    registry->RegisterReplicatedComponent<PositionComponent>("PositionComponent");
    registry->RegisterReplicatedComponent<TextComponent>("TextComponent");
    registry->RegisterReplicatedComponent<ModelComponent>("ModelComponent");
}

static void InitCommonECS(const flecs::world& ecs)
{
    InitPhases(ecs);
}

static void InitServerECS(const flecs::world& ecs)
{
    ecs.entity()
        .set<ReplicatedComponent>({})
        .set<PositionComponent>({20, 20, 0})
        .set<TextComponent>("");

    static bool hasCreatedBuildings{false};
    static auto serverStartTime = std::chrono::steady_clock::now();
    ecs.system<TextComponent>("UpdateUptimeText")
        .with<ReplicatedComponent>()
        .each([](const flecs::entity e, TextComponent& textComponent)
        {
            const auto now = std::chrono::steady_clock::now();
            int seconds = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(now - serverStartTime).count());
            const std::string text = fmt::format("Time elapsed since server start: {:d}s", seconds);
            // Only change the component trigger replication if the string has changed
            if (strcmp(text.c_str(), textComponent.mText) != 0)
            {
                std::snprintf(textComponent.mText, sizeof(textComponent.mText), "%s", text.c_str());
                e.modified<TextComponent>();
            }

            if (hasCreatedBuildings == false && seconds > 5)
            {
                e.world().entity()
                    .set<ReplicatedComponent>({})
                    .set<PositionComponent>({1.0, 0, 1.0})
                    .set<ModelComponent>({Assets::MakeAssetId("building_A", ModelAsset::Type)});

                e.world().entity()
                    .set<ReplicatedComponent>({})
                    .set<PositionComponent>({3.0, 0, 1.0})
                    .set<ModelComponent>({Assets::MakeAssetId("building_B", ModelAsset::Type)});

                e.world().entity()
                    .set<ReplicatedComponent>({})
                    .set<PositionComponent>({5.0, 0, 1.0})
                    .set<ModelComponent>({Assets::MakeAssetId("building_C", ModelAsset::Type)});

                spdlog::info("Created building entities");
                hasCreatedBuildings = true;
            }
        });
}

static void InitClientECS(const flecs::world& ecs)
{
    Camera3D camera3D = {0};
    camera3D.position = {0.0f, 10.0f, 10.0f};
    camera3D.target = {0.0f, 0.0f, 0.0f};
    camera3D.up = {0.0f, 1.0f, 0.0f};
    camera3D.fovy = 45.0f;
    camera3D.projection = CAMERA_PERSPECTIVE;

    ecs.set<CameraComponent>({camera3D});

    gPreDrawSystem = ecs.system<CameraComponent>()
                         .kind(PreDraw)
                         .each([](CameraComponent& camera) {
                             if (IsCursorHidden())
                             {
                                 UpdateCamera(&camera.mCamera, CAMERA_FREE);
                             }

                             if (IsKeyPressed(KEY_C))
                             {
                                 IsCursorHidden() ? EnableCursor() : DisableCursor();
                             }

                             BeginDrawing();
                             ClearBackground(LIGHTGRAY);
                         });

    ecs.system<const CameraComponent>("BeginDraw3D")
        .kind(Draw3D)
        .each([](const CameraComponent& camera) {
            BeginMode3D(camera.mCamera);
            DrawGrid(20, 2.0f);
        });

    ecs.system<const ModelComponent>("LoadModels")
        .with<PositionComponent>()
        .without<ModelInstanceComponent>()
        .kind(Draw3D)
        .each([](const flecs::entity e, const ModelComponent& model) {
            const ModelAsset* asset = static_cast<ModelAsset*>(Assets::GetAsset(model.mModelAssetId));
            e.set<ModelInstanceComponent>({
                .mModel = LoadModel(asset->mModelPath.c_str()),
                .mScale = model.mScale,
                .mTint = model.mTint
            });
        });

    ecs.system<const PositionComponent, const ModelInstanceComponent>("DrawModels")
        .kind(Draw3D)
        .each([](const PositionComponent& position, const ModelInstanceComponent& model) {
            DrawModel(model.mModel, position.mPosition, model.mScale, model.mTint);
        });

    ecs.system("EndDraw3D").kind(Draw3D).each([]() { EndMode3D(); });

    ecs.system<const PositionComponent, const TextComponent>("DrawText")
        .kind(Draw2D)
        .each([](const PositionComponent& position,
                 const TextComponent& text) { DrawText(text.mText, static_cast<int>(position.mPosition.x), static_cast<int>(position.mPosition.y), 30.0, BLACK); });

    gEndDrawSystem = ecs.system().kind(PostDraw).each([]() { EndDrawing(); });
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

}; // namespace fc::Core
