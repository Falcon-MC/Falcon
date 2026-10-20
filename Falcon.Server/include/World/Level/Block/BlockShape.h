#pragma once

#include "World/Level/Block/BlockState.h"
#include "Core/Math/AxisAlignedBB.h"

namespace BlockShape {

    bool hasCollision(const BlockState &state);

    bool isFullCube(const BlockState &state);

    AxisAlignedBB getRelativeShape(const BlockState &state);

    AxisAlignedBB getShapeAt(const BlockState &state, int32_t x, int32_t y, int32_t z);

    bool isPositionInside(const BlockState &state, int32_t blockX, int32_t blockY, int32_t blockZ,
                          float x, float y, float z);

}
