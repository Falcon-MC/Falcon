#include "World/Level/LevelGen/Overworld/Feature/Decoration/VegetationPatchFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/LevelGen/Noise/SimplexF.h"
#include "World/Level/LevelGen/Feature/FeatureNoiseHolder.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <memory>

namespace {

    const BlockState &mossState() {
        static const BlockState state = VanillaBlocks::MOSS_BLOCK().toBlockState();
        return state;
    }

    const BlockState &shortGrassState() {
        static const BlockState state = VanillaBlocks::SHORT_GRASS().toBlockState();
        return state;
    }

    const BlockState &lowerTallGrassState() {
        static const BlockState state = DecorationSupport::withByteState(
                VanillaBlocks::TALL_GRASS().toBlockState(), "upper_block_bit", false);
        return state;
    }

    const BlockState &upperTallGrassState() {
        static const BlockState state = DecorationSupport::withByteState(
                VanillaBlocks::TALL_GRASS().toBlockState(), "upper_block_bit", true);
        return state;
    }

    const BlockState &mossCarpetState() {
        static const BlockState state = VanillaBlocks::MOSS_CARPET().toBlockState();
        return state;
    }

    const BlockState &azaleaState() {
        static const BlockState state = VanillaBlocks::AZALEA().toBlockState();
        return state;
    }

    const BlockState &floweringAzaleaState() {
        static const BlockState state = VanillaBlocks::FLOWERING_AZALEA().toBlockState();
        return state;
    }

    const SimplexF *patchNoise() {
        const FeatureNoiseHolder *holder = FeatureNoiseHolder::get();
        if (holder == nullptr)
            return nullptr;

        return &holder->getMossPatchSnapToFloor();
    }

}

const char *VegetationPatchFeature::name() const {
    return "minecraft:moss_patch_snap_to_floor_feature";
}

std::vector<int32_t> VegetationPatchFeature::_getHighestWorkableBlocks(LevelChunk &chunk, int32_t x, int32_t z) {
    std::vector<int32_t> blockYs;

    for (int32_t y = chunk.getHeight(x, z); y > LevelChunk::MIN_Y; --y) {
        if ((int32_t) chunk.getBiomeAt(x, y, z) != BiomeIds::LUSH_CAVES)
            continue;

        const std::string &id = chunk.getBlock(x, y, z).mName;
        if ((id == "minecraft:stone" || id == "minecraft:deepslate")
            && DecorationSupport::isAir(chunk.getBlock(x, y + 1, z)))
            blockYs.push_back(y);
    }

    return blockYs;
}

void VegetationPatchFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    mRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunkX, chunkZ));
    const SimplexF *noisePtr = patchNoise();
    if (noisePtr == nullptr)
        return;

    const SimplexF &noise = *noisePtr;

    BlockManager manager(level);

    for (int32_t x = 0; x < 16; x++) {
        const int32_t baseX = (chunkX << 4) + x;
        for (int32_t z = 0; z < 16; z++) {
            const int32_t baseZ = (chunkZ << 4) + z;
            if (noise.noise2D((float) baseX, (float) baseZ, true) <= 0.0f)
                continue;

            for (int32_t y: _getHighestWorkableBlocks(chunk, x, z)) {
                manager.setBlockStateAt(baseX, y, baseZ, mossState());
                const int32_t value = mRandom.nextInt(20);

                if (value >= 0 && value <= 4) {
                    chunk.setBlock(x, y + 1, z, shortGrassState());
                } else if (value >= 5 && value <= 9) {
                    manager.setBlockStateAt(baseX, y + 1, baseZ, lowerTallGrassState());
                    manager.setBlockStateAt(baseX, y + 2, baseZ, upperTallGrassState());
                } else if (value >= 10 && value <= 13) {
                    manager.setBlockStateAt(baseX, y + 1, baseZ, mossCarpetState());
                } else if (value == 14) {
                    manager.setBlockStateAt(baseX, y + 1, baseZ,
                                            mRandom.nextBoolean() ? azaleaState() : floweringAzaleaState());
                }
            }
        }
    }

    queueObject(manager);
}
