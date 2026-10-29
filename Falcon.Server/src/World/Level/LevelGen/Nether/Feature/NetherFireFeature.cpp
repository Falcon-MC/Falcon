#include "World/Level/LevelGen/Nether/Feature/NetherFireFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {
    const BlockState &fireState() {
        static const BlockState state = VanillaBlocks::FIRE().toBlockState();
        return state;
    }

    const BlockState &soulFireState() {
        static const BlockState state = VanillaBlocks::SOUL_FIRE().toBlockState();
        return state;
    }
}

const char *NetherFireFeature::name() const {
    return "nether_fire";
}

std::vector<int32_t> NetherFireFeature::_getHighestWorkableBlocks(BlockManager &manager, int32_t x, int32_t z) {
    std::vector<int32_t> blockYs;

    for (int32_t y = 128; y > 0; y--) {
        const BlockState &state = manager.getBlockAt(x, y, z);
        const bool support = isName(state, "minecraft:netherrack")
                             || isName(state, "minecraft:soul_sand")
                             || isName(state, "minecraft:soul_soil");

        if (support && isAir(manager.getBlockAt(x, y + 1, z)))
            blockYs.push_back(y + 1);
    }

    return blockYs;
}

void NetherFireFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    seedForChunk(level, chunkX, chunkZ);

    BlockManager object(level);
    const int32_t amount = mRandom.nextInt(2) + 1;

    for (int32_t i = 0; i < amount; i++) {
        const int32_t x = randomRange(mRandom, chunkX << 4, (chunkX << 4) + 15);
        const int32_t z = randomRange(mRandom, chunkZ << 4, (chunkZ << 4) + 15);
        const std::vector<int32_t> blockYs = _getHighestWorkableBlocks(object, x, z);

        for (int32_t y: blockYs) {
            if (y <= 1)
                continue;

            if (mRandom.nextInt(4) == 1)
                continue;

            const bool onNetherrack = isName(object.getBlockAt(x, y - 1, z), "minecraft:netherrack");
            object.setBlockStateAt(x, y, z, onNetherrack ? fireState() : soulFireState());
        }
    }

    queueObject(object);
}
