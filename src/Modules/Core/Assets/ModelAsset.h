#pragma once

#include <filesystem>

#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <raylib.h>

#include "Utils/Json.h"
#include "Assets/AssetTypes.h"

namespace fc::Core
{

struct ModelAsset : Assets::Asset
{
    float mScale = 1.0f;
    Color mTint = WHITE;

    std::string mModelPath;

    static Asset* FromJson(const nlohmann::json& json, const std::string& directoryPath)
    {
        ModelAsset asset;
        Assets::FromJson(json, &asset);

        asset.mScale = json.value("scale", 1.0f);

        const std::filesystem::path defaultModelPath = (std::filesystem::path(directoryPath) / fmt::format("{}.gltf", asset.mIdString)).lexically_normal();
        asset.mModelPath = json.value("modelPath", defaultModelPath);

        if (json.contains("tint"))
            Utils::Json::ParseColor(json.at("tint"), asset.mTint);

        return new ModelAsset{std::move(asset)};
    }
};

constexpr Assets::AssetType ModelAssetType{
    .mName = "model",
    .FromJson = &ModelAsset::FromJson
};

} // namespace fc::Core