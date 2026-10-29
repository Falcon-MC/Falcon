#include "World/Level/LevelGen/Nether/Feature/BasaltDelta/DeltaMagmaFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Nether/NetherBiomeIds.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {
    const BlockState &magmaState() {
        static const BlockState state = VanillaBlocks::MAGMA().toBlockState();
        return state;
    }

    bool isLavaLike(const BlockState &state) {
        return state.mName == "minecraft:lava" || state.mName == "minecraft:flowing_lava";
    }
}

const char *DeltaMagmaFeature::name() const {
    return "nether_basalt_delta_magma";
}

std::vector<int32_t> DeltaMagmaFeature::_getHighestWorkableBlocks(BlockManager &manager, int32_t x, int32_t z) {
    std::vector<int32_t> blockYs;

    for (int32_t y = 128; y > 0; y--) {
        const BlockState &state = manager.getBlockAt(x, y, z);
        const bool support = isName(state, "minecraft:basalt") || isName(state, "minecraft:blackstone");
        if (!support)
            continue;

        if (!isAir(manager.getBlockAt(x, y + 1, z)))
            continue;

        const bool nextToLava = isLavaLike(manager.getBlockAt(x + 1, y, z))
                                || isLavaLike(manager.getBlockAt(x - 1, y, z))
                                || isLavaLike(manager.getBlockAt(x, y, z + 1))
                                || isLavaLike(manager.getBlockAt(x, y, z - 1));

        if (nextToLava)
            blockYs.push_back(y);
    }

    return blockYs;
}

void DeltaMagmaFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    seedForChunk(level, chunkX, chunkZ);

    BlockManager object(level);
    const int32_t amount = mRandom.nextBoundedInt(4) + 20;

    for (int32_t i = 0; i < amount; i++) {
        const int32_t x = randomRange(mRandom, chunkX << 4, (chunkX << 4) + 15);
        const int32_t z = randomRange(mRandom, chunkZ << 4, (chunkZ << 4) + 15);
        if (getBiomeIdAt(level, x, z) != NetherBiomeIds::BASALT_DELTAS)
            continue;

        const std::vector<int32_t> blockYs = _getHighestWorkableBlocks(object, x, z);
        for (int32_t y: blockYs) {
            if (y <= 1)
                continue;

            object.setBlockStateAt(x, y, z, magmaState());
        }
    }

    queueObject(object);
}
