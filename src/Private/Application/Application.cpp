#include "Application.h"

#include <enet/enet.h>
#include <nlohmann/json.hpp>
#include <raylib.h>

#include "Assets/Assets.h"
#include "Logging/Utils.h"
#include "Network/ClientThread.h"
#include "Network/ReplicationRequest.h"

namespace fc
{

constexpr int DEFAULT_WINDOW_WIDTH{1920};
constexpr int DEFAULT_WINDOW_HEIGHT{1080};

constexpr const char* DEFAULT_ASSET_ROOT{"assets"};

Application::Application()
{
    mComponentRegistry = new ECS::ComponentRegistry(mEcs);
}

Application::~Application()
{
    delete mComponentRegistry;
}

int Application::Run(fc::Environment::Options& options, std::vector<Module>& modules)
{
    fc::Logging::Initialise();

    spdlog::info("Registering asset types...");
    for (const auto module : modules)
    {
        module.RegisterAssetTypes();
    }

    spdlog::info("Initialising asset registry...");
    if (!Assets::Initialise(DEFAULT_ASSET_ROOT))
    {
        spdlog::error("Asset registry initialisation failed. Aborting startup.");
        return -1;
    }

    spdlog::info("Registering components...");
    for (const auto module : modules)
    {
        module.RegisterComponents(mComponentRegistry);
    }

    // For client and monolith modes
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    int status = 0;
    if (options.IsServer() || options.IsClient())
    {
        if (options.IsServer())
        {
            status = RunAsServer(options, modules);
        }
        else if (options.IsClient())
        {
            status = RunAsClient(options, modules);
        }
    }
    else
    {
        status = RunAsMonolith(options, modules);
    }

    for (const auto module : modules)
    {
        module.Cleanup(mEcs);
    }

    Assets::Shutdown();

    return status;
}

int Application::RunAsServer(fc::Environment::Options& options, std::vector<Module>& modules)
{
    // TODO: Implement basic CLI commands to do basic server ops.
    fc::Network::ServerThread serverThread(options.GetListenPort());
    serverThread.Start();

    spdlog::info("Initialising ECS...");
    for (auto module : modules)
    {
        module.InitCommonECS(mEcs);
        module.InitServerECS(mEcs);
    }

    spdlog::info("Entering main loop...");
    while (!mShouldQuit)
    {
        mEcs.progress();
        UpdateReplication(serverThread);
    }

    return 0;
}

int Application::RunAsClient(fc::Environment::Options& options, std::vector<Module>& modules)
{
    fc::Network::ClientThread clientThread;
    clientThread.Start();

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, "Flecs City");
    SetTargetFPS(60);

    Environment::ConnectAddress connectAddress = options.GetConnectAddress();
    if (!clientThread.Connect(connectAddress.mHostString.c_str(), connectAddress.mPort))
        return -1;

    spdlog::info("Initialising ECS...");
    for (auto module : modules)
    {
        module.InitCommonECS(mEcs);
        module.InitClientECS(mEcs);
    }

    spdlog::info("Entering main loop...");
    while (!WindowShouldClose())
    {
        clientThread.ProcessReplicationQueue(mComponentRegistry);
        mEcs.progress();
    }

    CloseWindow();

    return 0;
}

int Application::RunAsMonolith(fc::Environment::Options& options, std::vector<Module>& modules)
{
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, "Flecs City");
    SetTargetFPS(60);

    spdlog::info("Initialising ECS...");
    for (const auto module : modules)
    {
        module.InitCommonECS(mEcs);
        module.InitServerECS(mEcs);
        module.InitClientECS(mEcs);
    }

    spdlog::info("Entering main loop...");
    while (!WindowShouldClose())
    {
        mEcs.progress();
    }

    CloseWindow();

    return 0;
}

void Application::UpdateReplication(fc::Network::ServerThread& serverThread) const
{
    mEcs.each<ReplicatedComponent>([&](const flecs::entity e, ReplicatedComponent& rep)
    {
        if (!rep.mIsDirty) return;

        std::vector<flecs::id_t> dirtyComponents(
            rep.mDirtyComponents,
            rep.mDirtyComponents + rep.mDirtyComponentCount
        );

        auto request = Network::GenerateReplicationRequest(e, rep, false, dirtyComponents, mComponentRegistry);
        serverThread.QueueReplicationRequest(request);
        rep.ClearDirty();
    });

    std::vector<ENetPeer*> newPeers = serverThread.PopNewPeers();
    if (!newPeers.empty())
    {
        mEcs.each<ReplicatedComponent>([&](flecs::entity e, ReplicatedComponent& rep)
        {
            const std::unordered_set<flecs::id_t>& ids = mComponentRegistry->GetReplicatedComponents();

            std::vector<flecs::id_t> components(ids.begin(), ids.end());

            auto request = Network::GenerateReplicationRequest(e, rep, true, components, mComponentRegistry);

            for (ENetPeer* peer : newPeers)
            {
                Network::ReplicationRequest clientRequest = request;
                clientRequest.mRecipient = peer;
                serverThread.QueueReplicationRequest(clientRequest);
            }
        });
    }
}

} // namespace fc
