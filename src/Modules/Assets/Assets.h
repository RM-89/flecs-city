#pragma once

#include <Module/Macros.h>

#ifdef EXPORTS
#define ASSETS_API API_EXPORT
#else
#define ASSETS_API API_IMPORT
#endif

#include "AssetTypes.h"

namespace fc::Assets
{

/// @brief Registers an asset type. Must be called before Initialise.
ASSETS_API void RegisterType(AssetType type);

/// @brief Recursively scans the given asset root for valid asset manifests and builds the asset registry.
///
/// @param assetRoot Directory to scan. Must not be null.
/// @return True if the registry was built successfully.
ASSETS_API bool Initialise(const char* assetRoot);

/// @brief Returns the registered asset with the given ID, or nullptr if not found.
ASSETS_API Asset* GetAsset(AssetId id);

/// @brief Destroys the registry. Safe to call when not initialised.
ASSETS_API void Shutdown();

} // namespace fc::Assets