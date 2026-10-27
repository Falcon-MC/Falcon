#include "World/Level/LevelGen/End/Object/EndSpikeObject.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/End/TheEndGeneratorConstants.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"

#include <cstdlib>

namespace {

    const BlockState &obsidianState() {
        static const BlockState state = VanillaBlocks::OBSIDIAN().toBlockState();
        return state;
    }

    const BlockState &airState() {
        static const BlockState state = VanillaBlocks::AIR().toBlockState();
        return state;
    }

    const BlockState &ironBarsState() {
        static const BlockState state = VanillaBlocks::IRON_BARS().toBlockState();
        return state;
    }

    const BlockState &fireState() {
        static const BlockState state = VanillaBlocks::FIRE().toBlockState();
        return state;
    }

    const BlockState &infiniburnBedrockState() {
        static const BlockState state = []() {
            BlockState base = VanillaBlocks::BEDROCK().toBlockState();
            base.mStates.putByte("infiniburn_bit", 1);
            return BlockState(base.mName, base.mStates);
        }();
        return state;
    }

}

EndSpikeObject::EndSpikeObject(int32_t radius, int32_t height, bool guarded)
        : mRadius(radius), mHeight(height), mGuarded(guarded) {
}

bool EndSpikeObject::generate(BlockManager &manager, IRandom &, int32_t x, int32_t, int32_t z) {
    for (int32_t i = TheEndGeneratorConstants::MIN_Y; i <= mHeight + 10; i++) {
        for (int32_t j = -mRadius; j <= mRadius; j++) {
            for (int32_t k = -mRadius; k <= mRadius; k++) {
                if (j * j + k * k <= mRadius * mRadius + 1 && i < mHeight) {
                    manager.setBlockStateAt(x + j, i, z + k, obsidianState());
                } else if (i > 65) {
                    manager.setBlockStateAt(x + j, i, z + k, airState());
                }
            }
        }
    }

    if (mGuarded) {
        for (int32_t i = -2; i <= 2; ++i) {
            for (int32_t j = -2; j <= 2; ++j) {
                if (std::abs(i) == 2 || std::abs(j) == 2) {
                    for (int32_t k = 0; k < 3; ++k)
                        manager.setBlockStateAt(x + i, mHeight + k, z + j, ironBarsState());
                }

                manager.setBlockStateAt(x + i, mHeight + 3, z + j, ironBarsState());
            }
        }
    }

    manager.setBlockStateAt(x, mHeight, z, infiniburnBedrockState());
    manager.setBlockStateAt(x, mHeight + 1, z, fireState());
    return true;
}
