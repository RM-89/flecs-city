#pragma once

#include <string>

#include <args.hxx>
#include <utility>

#include "Environment/Constants.h"

namespace fc::Environment
{

struct ConnectAddress
{
    std::string mHostString;
    uint32_t mPort;

    ConnectAddress() : mPort(0) {}

    ConnectAddress(std::string hostString, const uint32_t port)
        : mHostString(std::move(hostString))
        , mPort(port)
    {
    }
};

const auto DEFAULT_CONNECT_ADDRESS = ConnectAddress("127.0.0.1", DEFAULT_LISTEN_PORT);

struct ConnectAddressReader
{
    void operator()(const std::string& name, const std::string& value, ConnectAddress& destination) const
    {
        const size_t colonPos = value.find_last_of(':');

        if (colonPos == std::string::npos)
        {
            if (value.empty())
            {
                throw args::ParseError("Invalid address format for '" + name + "': empty address");
            }
            destination = ConnectAddress(value, DEFAULT_LISTEN_PORT);
            return;
        }

        if (colonPos == 0)
        {
            throw args::ParseError("Invalid address format for '" + name + "': empty host in '" + value + "'");
        }

        const std::string host = value.substr(0, colonPos);

        if (colonPos == value.length() - 1)
        {
            destination = ConnectAddress(host, DEFAULT_LISTEN_PORT);
            return;
        }

        const std::string portStr = value.substr(colonPos + 1);

        try
        {
            const unsigned long portLong = std::stoul(portStr);

            if (portLong == 0 || portLong > 65535)
            {
                throw args::ParseError("Port number for '" + name + "' must be between 1 and 65535, got: " + portStr);
            }

            destination = ConnectAddress(host, static_cast<uint32_t>(portLong));
        }
        catch (const std::invalid_argument&)
        {
            throw args::ParseError("Invalid port number for '" + name + "': '" + portStr + "'");
        }
        catch (const std::out_of_range&)
        {
            throw args::ParseError("Port number for '" + name + "' out of range: '" + portStr + "'");
        }
    }
};

} // namespace fc::Environment
