#pragma once

#include "World/Level/Block/BlockState.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

class Level;

class BlockPlacementComponent {
public:
    static int getHorizontalFacing(float yaw);

    static BlockState apply(const BlockState &state, Level *level, float yaw, float pitch, int face,
                            const Vector3f &clickPosition, const Vector3f &playerPosition,
                            const Vector3i &blockPosition);
};
