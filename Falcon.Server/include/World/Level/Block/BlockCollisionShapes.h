#pragma once

#include "Core/Math/AxisAlignedBB.h"

#include <vector>

class BlockState;

// the vanilla collision boxes of every block state, BedrockData's block_collision_shapes.json
namespace BlockCollisionShapes {
    // local 0..1 boxes, empty for a state that does not collide; null for a state the data lacks
    const std::vector<AxisAlignedBB> *find(const BlockState &state);
}
