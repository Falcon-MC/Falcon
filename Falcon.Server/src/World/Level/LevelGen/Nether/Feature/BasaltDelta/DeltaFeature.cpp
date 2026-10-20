#include "World/Level/LevelGen/Nether/Feature/BasaltDelta/DeltaFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Nether/NetherBiomeIds.h"
#include "World/Level/LevelGen/Nether/NetherNoiseHolder.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {
    const BlockState &basaltState() {
        static const BlockState state = VanillaBlocks::BASALT().toBlockState();
        return state;
    }

    const BlockState &blackstoneState() {
        static const BlockState state = VanillaBlocks::BLACKSTONE().toBlockState();
        return state;
    }

    const BlockState &magmaState() {
        static const BlockState state = VanillaBlocks::MAGMA().toBlockState();
        return state;
    }

    const BlockState &lavaState() {
        static const BlockState state = VanillaBlocks::LAVA().toBlockState();
        return state;
    }

    const BlockState &flowingLavaState() {
        static const BlockState state = VanillaBlocks::FLOWING_LAVA().toBlockState();
        return state;
    }

    const int32_t HORIZONTAL_OFFSETS[4][2] = {
            {0,  -1},
            {0,  1},
            {-1, 0},
            {1,  0}
    };
}

const char *DeltaFeature::name() const {
    return "nether_basalt_delta_lava";
}

std::vector<int32_t> DeltaFeature::_getHighestWorkableBlocks(BlockManager &manager, int32_t x, int32_t z) {
    std::vector<int32_t> blockYs;

    for (int32_t y = 128; y > 0; y--) {
        const BlockState &state = manager.getBlockAt(x, y, z);
        const bool support = isName(state, "minecraft:basalt") || isName(state, "minecraft:blackstone");

        if (support
            && isAir(manager.getBlockAt(x, y + 1, z))
            && !isAir(manager.getBlockAt(x + 1, y, z))
            && !isAir(manager.getBlockAt(x - 1, y, z))
            && !isAir(manager.getBlockAt(x, y, z + 1))
            && !isAir(manager.getBlockAt(x, y, z - 1)))
            blockYs.push_back(y);
    }

    return blockYs;
}

void DeltaFeature::apply(ChunkGenerateContext &context) {
    const NetherNoiseHolder *noiseHolder = NetherNoiseHolder::get();
    if (noiseHolder == nullptr)
        return;

    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    seedForChunk(level, chunkX, chunkZ);

    const NetherDeltaNoises &noises = noiseHolder->getDeltaNoises();
    const NormalNoisePtr &surfaceNoise = noises.getSurfaceNoise();
    const NormalNoisePtr &surfaceSecondaryNoise = noises.getSurfaceSecondaryNoise();

    BlockManager object(level);
    const int32_t amount = mRandom.nextInt(64) + 64;

    for (int32_t i = 0; i < amount; i++) {
        const int32_t x = randomRange(mRandom, chunkX << 4, (chunkX << 4) + 15);
        const int32_t z = randomRange(mRandom, chunkZ << 4, (chunkZ << 4) + 15);
        if (getBiomeIdAt(level, x, z) != NetherBiomeIds::BASALT_DELTAS)
            continue;

        const std::vector<int32_t> blockYs = _getHighestWorkableBlocks(object, x, z);
        for (int32_t y: blockYs) {
            if (y <= 1)
                continue;

            object.setBlockStateAt(x, y, z, flowingLavaState());
        }
    }

    const int32_t baseX = chunkX << 4;
    const int32_t baseZ = chunkZ << 4;

    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++) {
            if ((int32_t) chunk.getBiomeAt(x, 0, z) != NetherBiomeIds::BASALT_DELTAS)
                continue;

            const int32_t worldX = x + baseX;
            const int32_t worldZ = z + baseZ;

            for (int32_t y = 1; y < 127; y++) {
                if (!isName(object.getBlockAt(worldX, y, worldZ), "minecraft:gravel"))
                    continue;

                const float secondary = surfaceSecondaryNoise->getValue(worldX, y, worldZ);
                const BlockState *replacement = &magmaState();
                if (secondary < -0.9f)
                    replacement = &blackstoneState();
                else if (secondary < 0.8f)
                    replacement = &basaltState();

                if (surfaceNoise->getValue(worldX, y, worldZ) > 0.0f) {
                    object.setBlockStateAt(worldX, y, worldZ, *replacement);
                    continue;
                }

                bool air = false;
                for (const int32_t *offset: HORIZONTAL_OFFSETS) {
                    if (isAir(object.getBlockAt(worldX + offset[0], y, worldZ + offset[1])))
                        air = true;
                }

                if (air)
                    object.setBlockStateAt(worldX, y, worldZ, *replacement);
                else
                    object.setBlockStateAt(worldX, y, worldZ, lavaState());
            }
        }
    }

    queueObject(object);
}
