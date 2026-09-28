#pragma once

#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>
#include <raylib.h>

#include "String.h"

namespace fc::Utils::Json
{

inline void ParseColor(const nlohmann::json& json, Color& color)
{
    auto str = json.get<std::string>();

    if (!str.empty() && str[0] == '#') str.erase(0, 1); // optional '#'

    if (str.size() != 6 && str.size() != 8)
        throw std::invalid_argument("Hex color must be RRGGBB or RRGGBBAA: " + str);

    color.r = String::ParseHexByte(str, 0);
    color.g = String::ParseHexByte(str, 2);
    color.b = String::ParseHexByte(str, 4);
    color.a = str.size() == 8 ? String::ParseHexByte(str, 6) : 255;
}

} // namespace fc::Utils::Json