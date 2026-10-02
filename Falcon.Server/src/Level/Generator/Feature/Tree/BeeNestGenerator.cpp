#include "Level/Generator/Feature/Tree/BeeNestGenerator.h"

#include "Actor/Mob/Neutral/BeeActor.h"
#include "Block/Actor/BeehiveBlockActor.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Level/Generator/Feature/BlockManager.h"
#include "Level/Generator/Feature/Tree/TreeBlockFace.h"
#include "Level/Level.h"

#include <utility>

namespace {

    const char *const ACTOR_IDENTIFIER_TAG = "identifier";
    const int32_t GENERATED_STAY_TICKS = 600;

    const char *FLOWER_BLOCKS[] = {
            "minecraft:dandelion",
            "minecraft:poppy",
            "minecraft:blue_orchid",
            "minecraft:allium",
            "minecraft:azure_bluet",
            "minecraft:red_tulip",
            "minecraft:orange_tulip",
            "minecraft:white_tulip",
            "minecraft:pink_tulip",
            "minecraft:oxeye_daisy",
            "minecraft:cornflower",
            "minecraft:lily_of_the_valley",
            "minecraft:wither_rose",
            "minecraft:torchflower",
            "minecraft:open_eyeblossom",
            "minecraft:closed_eyeblossom",
            "minecraft:flowering_azalea"
    };

    bool isFlower(const std::string &identifier) {
        for (const char *candidate: FLOWER_BLOCKS) {
            if (identifier == candidate)
                return true;
        }

        return false;
    }

}

bool BeeNestGenerator::place(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    return place(manager, x, y, z, random.nextInt(2, 4));
}

bool BeeNestGenerator::place(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t beeCount) {
    for (int32_t leafY = y + 1; leafY <= y + 32; leafY++) {
        const int32_t nestY = leafY - 1;
        const int32_t nestZ = z + 1;

        if (manager.getBlockAt(x, leafY - 1, z).mName == "minecraft:air"
            || manager.getBlockAt(x, nestY, nestZ).mName != "minecraft:air"
            || manager.getBlockAt(x, nestY + 1, nestZ).mName == "minecraft:air")
            continue;

        placeAt(manager, x, nestY, nestZ, beeCount);
        return true;
    }

    return false;
}

void BeeNestGenerator::placeAt(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t beeCount) {
    BlockState state = VanillaBlocks::BEE_NEST().toBlockState();
    state.mStates.putInt("direction", TreeBlockFaces::getHorizontalIndex(TreeBlockFace::SOUTH));
    state.mStates.putInt("honey_level", 0);
    manager.setBlockStateAt(x, y, z, BlockState(state.mName, state.mStates));

    BeehiveBlockActor &hive = manager.getLevel().getBlockActors().getOrCreate<BeehiveBlockActor>(Vector3i(x, y, z));
    for (int32_t bee = 0; bee < beeCount; bee++) {
        Tag saveData = Tag::ofCompound();
        saveData.putString(ACTOR_IDENTIFIER_TAG, BeeActor::IDENTIFIER);
        hive.addOccupant(BeeActor::IDENTIFIER, std::move(saveData), GENERATED_STAY_TICKS);
    }
}

bool BeeNestGenerator::hasNearbyFlower(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    for (int32_t currentX = x - 2; currentX <= x + 2; currentX++) {
        for (int32_t currentZ = z - 2; currentZ <= z + 2; currentZ++) {
            if (currentX == x && currentZ == z)
                continue;

            if (isFlower(manager.getBlockAt(currentX, y, currentZ).mName))
                return true;
        }
    }

    return false;
}
