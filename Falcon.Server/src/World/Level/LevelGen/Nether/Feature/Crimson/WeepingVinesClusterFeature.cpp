#include "World/Level/LevelGen/Nether/Feature/Crimson/WeepingVinesClusterFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Nether/NetherBiomeIds.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {
    const BlockState &weepingVinesState() {
        static const BlockState state = VanillaBlocks::WEEPING_VINES().toBlockState();
        return state;
    }

    const char *ENDING_BLOCKS[] = {
            "minecraft:netherrack",
            "minecraft:crimson_nylium",
            "minecraft:nether_wart_block",
            "minecraft:lava",
            "minecraft:flowing_lava",
            "minecraft:crimson_fungus",
            "minecraft:crimson_roots",
            "minecraft:quartz_ore",
            "minecraft:nether_gold_ore",
            "minecraft:ancient_debris"
    };

    bool isEndingBlock(const BlockState &state) {
        for (const char *candidate: ENDING_BLOCKS) {
            if (state.mName == candidate)
                return true;
        }

        return false;
    }
}

const char *WeepingVinesClusterFeature::name() const {
    return "nether_crimson_weeping_vines";
}

std::vector<int32_t> WeepingVinesClusterFeature::_getHighestWorkableBlocks(BlockManager &manager, int32_t x,
                                                                          int32_t z) {
    std::vector<int32_t> blockYs;

    for (int32_t y = 128; y > 0; y--) {
        const BlockState &state = manager.getBlockAt(x, y, z);
        const bool support = isName(state, "minecraft:crimson_nylium") || isName(state, "minecraft:netherrack");

        if (support && isAir(manager.getBlockAt(x, y - 1, z)))
            blockYs.push_back(y - 1);
    }

    return blockYs;
}

int32_t WeepingVinesClusterFeature::_getHighestEndingBlock(BlockManager &manager, int32_t x, int32_t y,
                                                          int32_t z) {
    for (; y > 0; y--) {
        const BlockState &state = manager.getBlockAt(x, y, z);
        if (isAir(manager.getBlockAt(x, y + 1, z)) && isEndingBlock(state))
            break;
    }

    return y + 1;
}

void WeepingVinesClusterFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    seedForChunk(level, chunkX, chunkZ);

    BlockManager object(level);
    const int32_t amount = mRandom.nextInt(5) + 1;

    for (int32_t i = 0; i < amount; i++) {
        const int32_t x = randomRange(mRandom, chunkX << 4, (chunkX << 4) + 15);
        const int32_t z = randomRange(mRandom, chunkZ << 4, (chunkZ << 4) + 15);
        if (getBiomeIdAt(level, x, z) != NetherBiomeIds::CRIMSON_FOREST)
            continue;

        const std::vector<int32_t> blockYs = _getHighestWorkableBlocks(object, x, z);
        for (int32_t y: blockYs) {
            if (y <= 1)
                continue;

            const int32_t endY = _getHighestEndingBlock(object, x, y, z);
            const int32_t amountToDecrease = mRandom.nextInt(y - endY);

            for (int32_t yPos = y; yPos > y - amountToDecrease; yPos--)
                object.setBlockStateAt(x, yPos, z, weepingVinesState());
        }
    }

    queueObject(object);
}
