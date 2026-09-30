#pragma once

#include <flecs.h>

/// @brief Component for designating an entity as replicated.
struct ReplicatedComponent
{
    bool mIsDirty = true;
    static constexpr size_t MAX_DIRTY_COMPONENTS = 64;
    flecs::id_t mDirtyComponents[MAX_DIRTY_COMPONENTS];
    size_t mDirtyComponentCount = 0;
    float mLastReplicatedTime{0};
    bool mIsNewEntity = true;

    void MarkDirty(const flecs::id_t componentId)
    {
        mIsDirty = true;

        if (IsComponentDirty(componentId)) return;

        if (mDirtyComponentCount < MAX_DIRTY_COMPONENTS)
        {
            mDirtyComponents[mDirtyComponentCount++] = componentId;
        }
    }

    void ClearDirty()
    {
        mIsDirty = false;
        mDirtyComponentCount = 0;
        mIsNewEntity = false;
    }

    [[nodiscard]] bool IsComponentDirty(const flecs::id_t componentId) const
    {
        if (mIsNewEntity) return true;

        for (size_t i = 0; i < mDirtyComponentCount; ++i)
        {
            if (mDirtyComponents[i] == componentId)
                return true;
        }

        return false;
    }
};
