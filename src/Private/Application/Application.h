#pragma once

#include <vector>

#include <flecs.h>

#include "ECS/ComponentRegistry.h"
#include "Environment/Options.h"
#include "Module/Module.h"
#include "Network/ServerThread.h"

namespace fc
{

class Application
{
public:
    static Application& GetInstance()
    {
        static Application instance;
        return instance;
    }

    Application(Application const&) = delete;
    void operator=(Application const&) = delete;

    int Run(fc::Environment::Options& options, std::vector<Module>& modules);

private:
    bool mShouldQuit{false};

    flecs::world mEcs;
    ECS::ComponentRegistry* mComponentRegistry;

    Application();
    ~Application();

    int RunAsServer(fc::Environment::Options& options, const std::vector<Module>& modules) const;
    int RunAsClient(fc::Environment::Options& options, const std::vector<Module>& modules) const;
    int RunAsMonolith(fc::Environment::Options& options, const std::vector<Module>& modules) const;

    void UpdateReplication(fc::Network::ServerThread& serverThread) const;
};

}  // namespace fc
