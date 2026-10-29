#include "World/Level/LevelGen/Nether/Object/NyliumVegetationGenerator.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"

namespace {
    const BlockState &crimsonFungusState() {
        static const BlockState state = VanillaBlocks::CRIMSON_FUNGUS().toBlockState();
        return state;
    }

    const BlockState &warpedFungusState() {
        static const BlockState state = VanillaBlocks::WARPED_FUNGUS().toBlockState();
        return state;
    }

    const BlockState &crimsonRootsState() {
        static const BlockState state = VanillaBlocks::CRIMSON_ROOTS().toBlockState();
        return state;
    }

    const BlockState &warpedRootsState() {
        static const BlockState state = VanillaBlocks::WARPED_ROOTS().toBlockState();
        return state;
    }

    const BlockState &netherSproutsState() {
        static const BlockState state = VanillaBlocks::NETHER_SPROUTS().toBlockState();
        return state;
    }
}

void NyliumVegetationGenerator::growVegetation(BlockManager &manager, int32_t x, int32_t y, int32_t z,
                                               IRandom &random) {
    for (int32_t i = 0; i < 128; i++) {
        int32_t num = 0;
        int32_t currentX = x;
        int32_t currentY = y + 1;
        int32_t currentZ = z;

        bool crimson = manager.getBlockAt(currentX, currentY - 1, currentZ).mName == "minecraft:crimson_nylium";

        while (true) {
            if (num >= i / 16) {
                if (manager.getBlockAt(currentX, currentY, currentZ).mName == "minecraft:air") {
                    if (crimson) {
                        if (random.nextInt(8) == 0) {
                            if (random.nextInt(8) == 0)
                                manager.setBlockStateAt(currentX, currentY, currentZ, warpedFungusState());
                            else
                                manager.setBlockStateAt(currentX, currentY, currentZ, crimsonFungusState());
                        } else {
                            manager.setBlockStateAt(currentX, currentY, currentZ, crimsonRootsState());
                        }
                    } else {
                        if (random.nextInt(8) == 0) {
                            if (random.nextInt(8) == 0)
                                manager.setBlockStateAt(currentX, currentY, currentZ, crimsonFungusState());
                            else
                                manager.setBlockStateAt(currentX, currentY, currentZ, warpedFungusState());
                        } else {
                            if (random.nextBoolean())
                                manager.setBlockStateAt(currentX, currentY, currentZ, warpedRootsState());
                            else
                                manager.setBlockStateAt(currentX, currentY, currentZ, netherSproutsState());
                        }
                    }
                }

                break;
            }

            currentX += random.nextInt(-1, 1);
            currentY += random.nextInt(-1, 1) * random.nextInt(3) / 2;
            currentZ += random.nextInt(-1, 1);

            const std::string &identifier = manager.getBlockAt(currentX, currentY - 1, currentZ).mName;
            crimson = identifier == "minecraft:crimson_nylium";
            if ((!crimson && identifier != "minecraft:warped_nylium") || currentY > 255 || currentY < 0)
                break;

            num++;
        }
    }
}
