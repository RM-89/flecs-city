#pragma once

#include <raylib.h>

namespace fc
{

/// @brief Wraps a model instance loaded by Raylib. Should be initialised from a @c ModelComponent.
struct ModelInstanceComponent
{
    Model mModel;

    float mScale = 1.f;
    Color mTint = WHITE;
};

}; // namespace fc
