#include <vector>

#include "Application/Application.h"
#include "Environment/Options.h"
#include "Modules/Core/Core.h"

int main(const int argc, char** argv)
{
    fc::Environment::Options options;
    if (!options.Init(argc, argv))
        return -1;

    std::vector<fc::Module> modules{
        fc::Core::MODULE,
    };

    fc::Application& app = fc::Application::GetInstance();
    return app.Run(options, modules);
}
