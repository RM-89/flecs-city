#include "Options.h"


namespace fc::Environment
{

bool Options::Init(const int argc, char** argv)
{
    try
    {
        mParser.ParseCLI(argc, argv);
    }
    catch (args::Help&)
    {
        std::cout << mParser;
        return false;
    }
    catch (args::ParseError& e)
    {
        std::cerr << e.what() << std::endl;
        std::cerr << mParser;
        return false;
    }
    catch (args::ValidationError& e)
    {
        std::cerr << e.what() << std::endl;
        std::cerr << mParser;
        return false;
    }

    return true;
}

bool Options::IsMonolith()
{
    return args::get(mMode) == RunMode::Monolith;
}

bool Options::IsServer()
{
    return args::get(mMode) == RunMode::Server;
}

bool Options::IsClient()
{
    return args::get(mMode) == RunMode::Client;
}

uint32_t Options::GetListenPort()
{
    return args::get(mListen);
}

ConnectAddress Options::GetConnectAddress()
{
    return args::get(mConnect);
}

} // namespace fc::Environment
