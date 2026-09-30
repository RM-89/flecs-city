#include "Assets.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "AssetTypes.h"

namespace fs = std::filesystem;

namespace fc::Assets
{

namespace
{

/// @brief Suffix identifying a manifest file within the asset tree.
constexpr const char* MANIFEST_SUFFIX = ".json";

/// @brief Represents a registered asset type.
struct AssetType
{
    DeserialiseFunc mDeserialise;
};

std::unordered_map<std::string, AssetType> gTypes = {};

struct Registry
{
    std::string mRoot;
    std::vector<Asset*> mAssets;
    std::unordered_map<AssetId, uint32_t> mIndexById;

    void Initialise()
    {
        for (uint32_t i = 0; i < mAssets.size(); ++i)
        {
            if (!mIndexById.emplace(mAssets[i]->mId, i).second)
            {
                spdlog::error("Internal error: asset id {} ('{}') collided in the registry index.",
                              mAssets[i]->mId, mAssets[i]->mIdString);
            }
        }
    }
};

Registry gRegistry;

/// @brief Validates the asset root and returns it in a normalised form.
std::optional<fs::path> ResolveRoot(const char* assetRoot)
{
    if (assetRoot == nullptr || *assetRoot == '\0')
    {
        spdlog::error("Asset root must be a non-empty path.");
        return std::nullopt;
    }

    const auto root = fs::path(assetRoot);

    std::error_code ec;
    if (!fs::is_directory(root, ec))
    {
        spdlog::error("Asset root '{}' is not a directory.", root.string());
        return std::nullopt;
    }

    return root.lexically_normal();
}

Asset* LoadAsset(const fs::path& manifestPath, const fs::path& root)
{
    std::error_code ec;
    const fs::path relativePath = fs::relative(manifestPath, root, ec);
    const std::string manifestPathRelative = relativePath.generic_string();
    const std::string manifestDirectoryPath = manifestPath.parent_path().generic_string();

    std::ifstream file(manifestPath);
    if (!file.is_open())
    {
        spdlog::error("Asset manifest '{}' could not be opened.", manifestPathRelative);
        return nullptr;
    }

    nlohmann::json manifest;
    try
    {
        manifest = nlohmann::json::parse(file);
    }
    catch (const nlohmann::json::exception& e)
    {
        spdlog::error("Asset manifest '{}' is not valid JSON: {}", manifestPathRelative, e.what());
        return nullptr;
    }

    const std::string typeName = manifest.value("type", std::string{});
    auto it = gTypes.find(typeName);
    if (it == gTypes.end())
    {
        spdlog::warn("Asset manifest with unknown type '{}'; skipping.", typeName);
        return nullptr;
    }

    const AssetType& type = it->second;
    try
    {
        Asset* asset = type.mDeserialise(manifest, manifestDirectoryPath);
        asset->mId = MakeAssetId(asset->mIdString.c_str(), typeName);
        asset->mRelativePath = manifestPathRelative;
        return asset;
    }
    catch (const std::exception& e)
    {
        spdlog::error("Asset manifest '{}' could not be loaded: {}", manifestPathRelative, e.what());
        return nullptr;
    }
}

bool CollectAssets(const fs::path& root, std::vector<Asset*>& outAssets)
{
    std::error_code errorCode;
    const size_t suffixLength = std::strlen(MANIFEST_SUFFIX);
    const fs::recursive_directory_iterator end;

    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, errorCode);
         it != end && !errorCode;
         it.increment(errorCode))
    {
        std::error_code entryErrorCode;
        if (!it->is_regular_file(entryErrorCode))
            continue;

        const std::string filename = it->path().filename().string();
        if (filename.size() <= suffixLength)
            continue;

        if (filename.compare(filename.size() - suffixLength, suffixLength, MANIFEST_SUFFIX) == 0)
        {
            if (Asset* asset = LoadAsset(it->path(), root))
            {
                const AssetId id = asset->mId;
                const bool duplicate = std::any_of(outAssets.begin(), outAssets.end(),
                                                   [id](const Asset* other) { return other->mId == id; });
                if (duplicate)
                {
                    spdlog::error("Asset manifest '{}' reuses asset id '{}' ({}); skipping.",
                                  asset->mRelativePath, asset->mIdString, id);
                    delete asset;
                    continue;
                }

                outAssets.push_back(asset);
            }
        }
    }

    if (errorCode)
    {
        spdlog::error("Failed while scanning asset root '{}': {}", root.string(), errorCode.message());
        return false;
    }

    std::sort(outAssets.begin(), outAssets.end(), [](const Asset* a, const Asset* b)
    {
        if (a->mId != b->mId) return a->mId < b->mId;
        return a->mRelativePath < b->mRelativePath;
    });

    return true;
}

}

void RegisterType(const std::string& name, const DeserialiseFunc deserialiseFunc)
{
    if (gTypes.find(name) != gTypes.end())
    {
        spdlog::error("Attempting to register duplicate asset type '{}'.", name);
        return;
    }

    gTypes.insert({name, {deserialiseFunc}});
}

bool Initialise(const char* assetRoot)
{
    Shutdown();

    const std::optional<fs::path> root = ResolveRoot(assetRoot);
    if (!root) return false;

    Registry registry;
    registry.mRoot = root->string();
    if (!CollectAssets(*root, registry.mAssets))
        return false;

    gRegistry = std::move(registry);
    gRegistry.Initialise();

    if (gRegistry.mAssets.empty())
        spdlog::warn("No assets found under '{}'.", gRegistry.mRoot);
    else
        spdlog::info("Registered {} asset(s) from '{}'.", gRegistry.mAssets.size(), gRegistry.mRoot);

    return true;
}

Asset* GetAsset(const AssetId id)
{
    const auto it = gRegistry.mIndexById.find(id);
    if (it == gRegistry.mIndexById.end())
        return nullptr;

    const auto index = it->second;
    return gRegistry.mAssets[index];
}

void Shutdown()
{
    for (const auto asset : gRegistry.mAssets)
    {
        delete asset;
    }
    gRegistry.mAssets.clear();
    gRegistry.mIndexById.clear();
}

} // namespace fc::Assets