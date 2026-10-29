#include "World/Level/LevelGen/Overworld/Feature/Decoration/MossCeilingPatchFeature.h"

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

    const BlockState &berryBodyState() {
        static const BlockState state = VanillaBlocks::CAVE_VINES_BODY_WITH_BERRIES().toBlockState();
        return state;
    }

    const BlockState &berryHeadState() {
        static const BlockState state = VanillaBlocks::CAVE_VINES_HEAD_WITH_BERRIES().toBlockState();
        return state;
    }

    const BlockState &vineState() {
        static const BlockState state = VanillaBlocks::CAVE_VINES().toBlockState();
        return state;
    }

    const BlockState &sporeState() {
        static const BlockState state = VanillaBlocks::SPORE_BLOSSOM().toBlockState();
        return state;
    }

    const SimplexF *ceilingNoise() {
        const FeatureNoiseHolder *holder = FeatureNoiseHolder::get();
        if (holder == nullptr)
            return nullptr;

        return &holder->getMossSnapToCeiling();
    }

}

const char *MossCeilingPatchFeature::name() const {
    return "minecraft:moss_ceiling_snap_to_ceiling_feature";
}

void MossCeilingPatchFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    mRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunkX, chunkZ));
    const SimplexF *noisePtr = ceilingNoise();
    if (noisePtr == nullptr)
        return;

    const SimplexF &noise = *noisePtr;

    BlockManager manager(level);

    for (int32_t x = 0; x < 16; x++) {
        const int32_t baseX = (x + chunkX) << 4;
        const int32_t worldX = (chunkX << 4) + x;

        for (int32_t z = 0; z < 16; z++) {
            const int32_t baseZ = (z + chunkZ) << 4;
            const int32_t worldZ = (chunkZ << 4) + z;

            if (noise.noise2D((float) baseX * 0.25f, (float) baseZ * 0.25f, true) <= 0.0f)
                continue;

            for (int32_t y = chunk.getHeight(x, z); y > LevelChunk::MIN_Y; y--) {
                if ((int32_t) chunk.getBiomeAt(x, y, z) != BiomeIds::LUSH_CAVES)
                    continue;

                if (!DecorationSupport::isAir(chunk.getBlock(x, y, z)))
                    continue;

                for (int32_t offsetY = 1; offsetY <= 2; offsetY++) {
                    const int32_t yy = y + offsetY;
                    const BlockState &state = chunk.getBlock(x, yy, z);
                    if (offsetY != 2)
                        continue;

                    if (state.mName != "minecraft:stone" && state.mName != "minecraft:deepslate")
                        continue;

                    manager.setBlockStateAt(worldX, yy, worldZ, mossState());
                    if (mRandom.nextFloat() >= 0.1f)
                        continue;

                    const float value = mRandom.nextFloat();
                    if (value < 0.003f) {
                        if (DecorationSupport::isAir(chunk.getBlock(x, yy - 1, z)))
                            manager.setBlockStateAt(worldX, yy - 1, worldZ, sporeState());

                        continue;
                    }

                    const int32_t targetBodyLength = mRandom.nextInt(0, 4);
                    int32_t lastPlacedY = yy;
                    bool placedAnyBody = false;

                    for (int32_t seg = 0; seg < targetBodyLength; seg++) {
                        const int32_t bodyY = yy - 1 - seg;
                        if (!DecorationSupport::isAir(chunk.getBlock(x, bodyY, z)))
                            break;

                        manager.setBlockStateAt(worldX, bodyY, worldZ,
                                                mRandom.nextFloat() > 0.3f ? vineState() : berryBodyState());
                        lastPlacedY = bodyY;
                        placedAnyBody = true;
                    }

                    const int32_t headY = lastPlacedY - 1;
                    if (DecorationSupport::isAir(chunk.getBlock(x, headY, z)))
                        manager.setBlockStateAt(worldX, headY, worldZ,
                                                mRandom.nextFloat() > 0.3f ? vineState() : berryHeadState());
                    else if (placedAnyBody)
                        manager.setBlockStateAt(worldX, lastPlacedY, worldZ,
                                                mRandom.nextFloat() > 0.3f ? vineState() : berryHeadState());
                }
            }
        }
    }

    queueObject(manager);
}
