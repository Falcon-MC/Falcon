#include "World/Level/LevelGen/Nether/Feature/BasaltDelta/BasaltPillarFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Nether/NetherBiomeIds.h"
#include "World/Level/LevelGen/Nether/NetherGeneratorConstants.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {
    const BlockState &basaltState() {
        static const BlockState state = VanillaBlocks::BASALT().toBlockState();
        return state;
    }
}

const char *BasaltPillarFeature::name() const {
    return "nether_basalt_delta_pillar";
}

std::vector<int32_t> BasaltPillarFeature::_getHighestWorkableBlocks(BlockManager &manager, int32_t x, int32_t z) {
    std::vector<int32_t> blockYs;

    for (int32_t y = 126; y > 1; y--) {
        if (isName(manager.getBlockAt(x, y, z), "minecraft:blackstone")
            && isAir(manager.getBlockAt(x, y + 1, z)))
            blockYs.push_back(y + 1);
    }

    return blockYs;
}

void BasaltPillarFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    seedForChunk(level, chunkX, chunkZ);

    BlockManager object(level);
    const int32_t amount = mRandom.nextBoundedInt(128) + 128;

    for (int32_t i = 0; i < amount; i++) {
        const int32_t x = randomRange(mRandom, chunkX << 4, (chunkX << 4) + 15);
        const int32_t z = randomRange(mRandom, chunkZ << 4, (chunkZ << 4) + 15);
        if (getBiomeIdAt(level, x, z) != NetherBiomeIds::BASALT_DELTAS)
            continue;

        const std::vector<int32_t> blockYs = _getHighestWorkableBlocks(object, x, z);
        for (int32_t y: blockYs) {
            if (y <= 1)
                continue;

            if (mRandom.nextBoundedInt(5) == 0)
                continue;

            for (int32_t randomHeight = 0; randomHeight < mRandom.nextBoundedInt(5) + 1; randomHeight++) {
                const int32_t placeLocation = y + randomHeight;
                if (placeLocation >= NetherGeneratorConstants::MAX_Y + 1)
                    break;

                object.setBlockStateAt(x, placeLocation, z, basaltState());
            }
        }
    }

    queueObject(object);
}
