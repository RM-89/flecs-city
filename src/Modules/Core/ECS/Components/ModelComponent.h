#pragma once

#include <raylib.h>

#include "Assets/AssetTypes.h"

namespace fc
{

/// @brief References a model asset by ID.
struct ModelComponent
{
    Assets::AssetId mModelAssetId;

    float mScale = 1.f;
    Color mTint = WHITE;
};

}; // namespace fc
