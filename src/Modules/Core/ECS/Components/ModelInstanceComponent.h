#pragma once

#include <raylib.h>

namespace fc
{

/// @brief Wraps a model instance loaded by Raylib. Should be initialised from a @c ModelComponent.
struct ModelInstanceComponent
{
    Model mModel;
    float mScale;
    Color mTint;
};

}; // namespace fc
