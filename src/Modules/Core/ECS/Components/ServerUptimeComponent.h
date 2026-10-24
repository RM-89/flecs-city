#pragma once

namespace fc
{

/// @brief Replicated singleton component for tracking server uptime.
struct ServerUptimeComponent
{
    int mUptime_s{0};
};

}; // namespace fc
