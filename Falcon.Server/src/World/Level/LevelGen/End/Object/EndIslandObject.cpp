#include "World/Level/LevelGen/End/Object/EndIslandObject.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"

#include <cmath>

namespace {

    const BlockState &endStoneState() {
        static const BlockState state = VanillaBlocks::END_STONE().toBlockState();
        return state;
    }

    int32_t floorFloat(float value) {
        return (int32_t) std::floor(value);
    }

    int32_t ceilFloat(float value) {
        return (int32_t) std::ceil(value);
    }

}

bool EndIslandObject::generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    float n = (float) (random.nextInt(2) + 4);

    for (int32_t offsetY = 0; n > 0.5f; offsetY--) {
        for (int32_t offsetX = floorFloat(-n); offsetX <= ceilFloat(n); offsetX++) {
            for (int32_t offsetZ = floorFloat(-n); offsetZ <= ceilFloat(n); offsetZ++) {
                if ((float) (offsetX * offsetX + offsetZ * offsetZ) <= (n + 1.0f) * (n + 1.0f))
                    manager.setBlockStateAt(x + offsetX, y + offsetY, z + offsetZ, endStoneState());
            }
        }

        n -= (float) ((double) random.nextInt(1) + 0.5f);
    }

    return true;
}
