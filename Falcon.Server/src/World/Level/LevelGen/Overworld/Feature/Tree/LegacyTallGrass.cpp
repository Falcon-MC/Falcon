#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacyTallGrass.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"

#include <cmath>
#include <vector>

namespace {

    const std::vector<BlockState> &places() {
        static const std::vector<BlockState> states = {
                VanillaBlocks::SHORT_GRASS().toBlockState(),
                VanillaBlocks::TALL_GRASS().toBlockState(),
                VanillaBlocks::DANDELION().toBlockState(),
                VanillaBlocks::POPPY().toBlockState(),
                VanillaBlocks::AZURE_BLUET().toBlockState(),
                VanillaBlocks::OXEYE_DAISY().toBlockState(),
                VanillaBlocks::ALLIUM().toBlockState(),
                VanillaBlocks::CORNFLOWER().toBlockState(),
                VanillaBlocks::BLUE_ORCHID().toBlockState(),
                VanillaBlocks::LILY_OF_THE_VALLEY().toBlockState(),
                VanillaBlocks::RED_TULIP().toBlockState(),
                VanillaBlocks::ORANGE_TULIP().toBlockState(),
                VanillaBlocks::PINK_TULIP().toBlockState(),
                VanillaBlocks::WHITE_TULIP().toBlockState()
        };
        return states;
    }

    BlockState tallGrassTopHalfState() {
        BlockState state = VanillaBlocks::TALL_GRASS().toBlockState();
        state.mStates.putByte("upper_block_bit", 1);
        return BlockState(state.mName, state.mStates);
    }

    int32_t javaRound(double value) {
        return (int32_t) std::floor(value + 0.5);
    }

}

void LegacyTallGrass::growGrass(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random) {
    const int32_t baseY = y + 1;
    const int32_t minX = x - 2;
    const int32_t minZ = z - 2;
    const int32_t maxX = x + 2;
    const int32_t maxZ = z + 2;

    for (int32_t currentX = minX; currentX <= maxX; currentX++) {
        for (int32_t currentZ = minZ; currentZ <= maxZ; currentZ++) {
            const int32_t newY = baseY + random.nextInt(2) * (random.nextBoolean() ? -1 : 1);
            if (!random.nextBoolean())
                continue;

            if (manager.getBlockAt(currentX, newY, currentZ).mName != "minecraft:air"
                || manager.getBlockAt(currentX, newY - 1, currentZ).mName != "minecraft:grass_block")
                continue;

            const int32_t ranNumber = javaRound(random.nextGaussian() * 1000);
            const int32_t absRn = std::abs(ranNumber);

            if (-300 <= ranNumber && ranNumber <= 300) {
                manager.setBlockStateAt(currentX, newY, currentZ, places()[0]);
            } else if (300 <= absRn && absRn <= 500) {
                manager.setBlockStateAt(currentX, newY, currentZ, places()[1]);
                manager.setBlockStateAt(currentX, newY + 1, currentZ, tallGrassTopHalfState());
            } else if (500 <= ranNumber && ranNumber < 600) {
                manager.setBlockStateAt(currentX, newY, currentZ, places()[2]);
            } else if (-600 <= ranNumber && ranNumber <= -500) {
                manager.setBlockStateAt(currentX, newY, currentZ, places()[3]);
            } else if (600 <= ranNumber && ranNumber < 700) {
                manager.setBlockStateAt(currentX, newY, currentZ, places()[4]);
            } else if (-700 <= ranNumber && ranNumber < -600) {
                manager.setBlockStateAt(currentX, newY, currentZ, places()[5]);
            } else if (-750 <= ranNumber && ranNumber < -700) {
                manager.setBlockStateAt(currentX, newY, currentZ, places()[6]);
            } else if (-800 <= ranNumber && ranNumber < -750) {
                manager.setBlockStateAt(currentX, newY, currentZ, places()[7]);
            } else if (-850 <= ranNumber && ranNumber < -800) {
                manager.setBlockStateAt(currentX, newY, currentZ, places()[8]);
            } else if (-900 <= ranNumber && ranNumber < -850) {
                manager.setBlockStateAt(currentX, newY, currentZ, places()[9]);
            } else if (-1000 <= ranNumber && ranNumber < -900) {
                manager.setBlockStateAt(currentX, newY, currentZ, places()[10]);
            } else if (700 <= ranNumber && ranNumber < 800) {
                manager.setBlockStateAt(currentX, newY, currentZ, places()[11]);
            } else if (800 <= ranNumber && ranNumber < 900) {
                manager.setBlockStateAt(currentX, newY, currentZ, places()[12]);
            } else if (900 <= ranNumber && ranNumber < 1000) {
                manager.setBlockStateAt(currentX, newY, currentZ, places()[13]);
            }
        }
    }
}
