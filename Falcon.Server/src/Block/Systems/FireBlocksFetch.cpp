#include "Block/Systems/FireBlocksFetch.h"

#include "Block/Block.h"
#include "Block/Blocks/FireBlock.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Level/Level.h"

#include <cmath>

namespace {
    const float PLAYER_HALF_WIDTH = 0.3f;
    const float PLAYER_HEIGHT = 1.8f;
    const float AABB_EPSILON = 0.001f;
}

FireContact FireBlocksFetch::at(Level &level, const Vector3f &feet) {
    FireContact contact;
    const int32_t minX = (int32_t) std::floor(feet.x - PLAYER_HALF_WIDTH);
    const int32_t maxX = (int32_t) std::floor(feet.x + PLAYER_HALF_WIDTH);
    const int32_t minY = (int32_t) std::floor(feet.y);
    const int32_t maxY = (int32_t) std::floor(feet.y + PLAYER_HEIGHT - AABB_EPSILON);
    const int32_t minZ = (int32_t) std::floor(feet.z - PLAYER_HALF_WIDTH);
    const int32_t maxZ = (int32_t) std::floor(feet.z + PLAYER_HALF_WIDTH);

    for (int32_t x = minX; x <= maxX; ++x) {
        for (int32_t y = minY; y <= maxY; ++y) {
            for (int32_t z = minZ; z <= maxZ; ++z) {
                const BlockState state = level.getBlockState(x, y, z);
                if (VanillaBlocks::getAs<FireBlock>(state.mName) == nullptr)
                    continue;

                return FireContact{true, state.mName == "minecraft:soul_fire" ? 2.0f : 1.0f};
            }
        }
    }

    return contact;
}
