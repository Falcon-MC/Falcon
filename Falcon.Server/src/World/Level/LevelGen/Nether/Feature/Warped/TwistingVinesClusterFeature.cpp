#include "World/Level/LevelGen/Nether/Feature/Warped/TwistingVinesClusterFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Nether/NetherBiomeIds.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {
    const BlockState &twistingVinesState() {
        static const BlockState state = VanillaBlocks::TWISTING_VINES().toBlockState();
        return state;
    }
}

const char *TwistingVinesClusterFeature::name() const {
    return "nether_warped_twisting_vines";
}

std::vector<int32_t> TwistingVinesClusterFeature::_getHighestWorkableBlocks(BlockManager &manager, int32_t x,
                                                                           int32_t z) {
    std::vector<int32_t> blockYs;

    for (int32_t y = 128; y > 0; y--) {
        const BlockState &state = manager.getBlockAt(x, y, z);
        const bool support = isName(state, "minecraft:warped_nylium")
                             || isName(state, "minecraft:warped_wart_block");

        if (support && canBeReplaced(manager.getBlockAt(x, y + 1, z)))
            blockYs.push_back(y + 1);
    }

    return blockYs;
}

int32_t TwistingVinesClusterFeature::_getHighestEndingBlock(BlockManager &manager, int32_t x, int32_t y,
                                                           int32_t z) {
    for (; y < 128; y++) {
        const BlockState &state = manager.getBlockAt(x, y, z);
        if (isAir(manager.getBlockAt(x, y - 1, z)) && isSolid(state))
            break;
    }

    return y - 1;
}

void TwistingVinesClusterFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    seedForChunk(level, chunkX, chunkZ);

    BlockManager object(level);
    const int32_t amount = mRandom.nextInt(6) + 2;

    for (int32_t i = 0; i < amount; i++) {
        const int32_t x = randomRange(mRandom, chunkX << 4, (chunkX << 4) + 15);
        const int32_t z = randomRange(mRandom, chunkZ << 4, (chunkZ << 4) + 15);
        if (getBiomeIdAt(level, x, z) != NetherBiomeIds::WARPED_FOREST)
            continue;

        const std::vector<int32_t> blockYs = _getHighestWorkableBlocks(object, x, z);
        for (int32_t y: blockYs) {
            if (y <= 1)
                continue;

            if (mRandom.nextInt(5) == 0)
                continue;

            const int32_t endY = _getHighestEndingBlock(object, x, y, z);
            const int32_t amountToDecrease = mRandom.nextInt(endY - y + 1);

            for (int32_t yPos = y; yPos < y + (amountToDecrease / 2); yPos++)
                object.setBlockStateAt(x, yPos, z, twistingVinesState());
        }
    }

    queueObject(object);
}
