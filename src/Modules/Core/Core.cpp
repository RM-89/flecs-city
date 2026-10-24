#include "Core.h"

#include <chrono>

#include <flecs.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <raylib.h>
#include <rcamera.h>
#include <rlgl.h>
#include <spdlog/spdlog.h>

#include "Assets/Assets.h"
#include "Assets/ModelAsset.h"

#include "ECS/ComponentRegistry.h"
#include "ECS/ReplicatedComponent.h"

#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/ModelComponent.h"
#include "ECS/Components/ModelInstanceComponent.h"
#include "ECS/Components/PositionComponent.h"
#include "ECS/Components/ServerUptimeComponent.h"
#include "ECS/Components/TextComponent.h"
#include "ECS/Phases.h"

namespace fc::Core
{

static constexpr float CAMERA_FOV = 60.0f;
static constexpr float CAMERA_LOOK_SPEED = 0.003f;
static constexpr float CAMERA_MOVE_SPEED = 40.0f;
static constexpr float CAMERA_SLOW_MOVE_SPEED = 20.0f;

static double gPreviousFrameTime = 0.0;
static float gFrameDelta = 0.0;

static void RegisterAssetTypes()
{
    Assets::RegisterType(ModelAsset::Type, &ModelAsset::Deserialise);
}

static void RegisterComponents(ECS::ComponentRegistry* registry)
{
    registry->RegisterComponent<ReplicatedComponent>();

    registry->RegisterComponent<CameraComponent>().add(flecs::Singleton);
    registry->RegisterComponent<ModelInstanceComponent>();

    registry->RegisterReplicatedSingleton<ServerUptimeComponent>("ServerUptimeComponent");
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
    ecs.set<ServerUptimeComponent>({});

    static auto serverStartTime = std::chrono::steady_clock::now();
    ecs.system<ServerUptimeComponent>("UpdateServerUptime")
        .each([&ecs](ServerUptimeComponent& component)
        {
            const auto now = std::chrono::steady_clock::now();
            const int uptime = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(now - serverStartTime).count());

            if (uptime != component.mUptime_s)
            {
                component.mUptime_s = uptime;
                ecs.modified<ServerUptimeComponent>();
            }
        });
}

static void InitClientECS(const flecs::world& ecs)
{
    gPreviousFrameTime = GetTime();

    Camera3D camera3D = {};
    camera3D.position = {0.0f, 10.0f, 10.0f};
    camera3D.target = {0.0f, 0.0f, 0.0f};
    camera3D.up = {0.0f, 1.0f, 0.0f};
    camera3D.fovy = CAMERA_FOV;
    camera3D.projection = CAMERA_PERSPECTIVE;

    ecs.set<CameraComponent>({camera3D});

    ecs.system("UpdateFrameDelta")
        .kind(PreDraw)
        .each([] {
            const double now = GetTime();
            gFrameDelta = static_cast<float>(now - gPreviousFrameTime);
            gPreviousFrameTime = now;
        });

    ecs.system<CameraComponent>("Update")
        .kind(PreDraw)
        .each([](CameraComponent& camera) {
            if (IsCursorHidden())
            {
                const auto [x, y] = GetMouseDelta();
                CameraYaw(&camera.mCamera, -x * CAMERA_LOOK_SPEED, false);
                CameraPitch(&camera.mCamera, -y * CAMERA_LOOK_SPEED, true, false, false);

                const float speed = (IsKeyDown(KEY_LEFT_SHIFT) ? CAMERA_SLOW_MOVE_SPEED : CAMERA_MOVE_SPEED) * gFrameDelta;
                if (IsKeyDown(KEY_W)) CameraMoveForward(&camera.mCamera, speed, false);
                if (IsKeyDown(KEY_S)) CameraMoveForward(&camera.mCamera, -speed, false);
                if (IsKeyDown(KEY_A)) CameraMoveRight(&camera.mCamera, -speed, false);
                if (IsKeyDown(KEY_D)) CameraMoveRight(&camera.mCamera, speed, false);
                if (IsKeyDown(KEY_SPACE)) CameraMoveUp(&camera.mCamera, speed);
                if (IsKeyDown(KEY_LEFT_CONTROL)) CameraMoveUp(&camera.mCamera, -speed);
            }

            if (IsKeyPressed(KEY_C))
            {
                IsCursorHidden() ? EnableCursor() : DisableCursor();
            }
        });

    ecs.system<const CameraComponent>("BeginDraw3D")
        .kind(Draw3D)
        .each([](const CameraComponent& camera) {
            BeginDrawing();
            BeginMode3D(camera.mCamera);
            ClearBackground(LIGHTGRAY);
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

    ecs.system("EndDraw3D")
        .kind(Draw3D)
        .each([] {
            EndMode3D();
        });

    ecs.system("StartImGui")
        .kind(DrawUI)
        .each([] {
            rlDrawRenderBatchActive();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
        });

    ecs.system<const ServerUptimeComponent>("DrawDebugUI")
        .kind(DrawUI)
        .each([](const ServerUptimeComponent& uptime) {
            ImGui::Begin("Debug");
            ImGui::Text("Server uptime: %d s", uptime.mUptime_s);
            ImGui::Text("Frame time: %.3f ms", gFrameDelta * 1000.0f);
            ImGui::End();
        });

    ecs.system("EndImGui")
        .kind(PostDraw)
        .each([] {
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        });

    ecs.system()
        .kind(PostDraw)
        .each([] {
            EndDrawing();

            SwapScreenBuffer();
            PollInputEvents();
        });
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
