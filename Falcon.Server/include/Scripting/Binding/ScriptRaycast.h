#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "World/Actor/ActorSize.h"

#include <cstdint>
#include <functional>
#include <string>

class Actor;
class BlockState;
class Level;

// the ray of BlockRaycastOptions and EntityRaycastOptions
namespace ScriptRaycast {
    // 250 blocks without maxDistance, never more than 1000
    constexpr int32_t DEFAULT_DISTANCE = 250;
    constexpr int32_t MAX_DISTANCE = 1000;

    struct BlockOptions {
        float mMaxDistance = (float) DEFAULT_DISTANCE;
        bool mIncludeLiquids = false;
        bool mIncludePassable = false;
        // the BlockFilter of the options: false skips the block, the ray goes on
        std::function<bool(const BlockState &)> mAccepts;
    };

    struct BlockHit {
        Vector3i mBlock;
        // Direction value
        const char *mFace = "Up";
        // relative to the block's corner
        Vector3f mFaceLocation;
        float mDistance = 0.0f;
    };

    // the distance option as the game clamps it; false when it allows no ray at all
    bool clampDistance(bool present, double requested, float &distance);

    // the first block on the ray; unloaded chunks stop it without a hit
    bool castBlocks(Level &level, const Vector3f &origin, const Vector3f &direction, const BlockOptions &options,
                    BlockHit &hit);

    // the collision box size: a player's follows its pose, an item entity is a quarter block
    ActorSize actorSize(const Actor &actor);

    // where the ray enters an axis aligned box, -1 when it misses; enteredAxis: the entered face's axis, 0 x, 1 y, 2 z
    float intersectBox(const Vector3f &origin, const Vector3f &direction, const Vector3f &low, const Vector3f &high,
                       int *enteredAxis = nullptr);
}
