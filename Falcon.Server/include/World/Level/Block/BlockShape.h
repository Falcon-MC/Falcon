#pragma once

#include "World/Level/Block/BlockState.h"
#include "Core/Math/AxisAlignedBB.h"

#include <cstdint>
#include <functional>
#include <vector>

class Actor;

// actor: who collides, null for none; some blocks shape themselves per actor type
namespace BlockShape {

    bool hasCollision(const BlockState &state, const Actor *actor = nullptr);

    bool isFullCube(const BlockState &state);

    AxisAlignedBB getRelativeShape(const BlockState &state, const Actor *actor = nullptr);

    AxisAlignedBB getShapeAt(const BlockState &state, int32_t x, int32_t y, int32_t z, const Actor *actor = nullptr);

    // the block at a position, null when unloaded
    using Neighbours = std::function<const BlockState *(int32_t, int32_t, int32_t)>;

    // every collision box of the block, in world coordinates
    void appendShapesAt(const BlockState &state, int32_t x, int32_t y, int32_t z, const Actor *actor,
                        const Neighbours &neighbours, std::vector<AxisAlignedBB> &out);

    bool isPositionInside(const BlockState &state, int32_t blockX, int32_t blockY, int32_t blockZ,
                          float x, float y, float z, const Actor *actor = nullptr);

}
