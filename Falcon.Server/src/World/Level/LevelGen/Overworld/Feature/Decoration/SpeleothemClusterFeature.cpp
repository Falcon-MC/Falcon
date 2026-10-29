#include "World/Level/LevelGen/Overworld/Feature/Decoration/SpeleothemClusterFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/LevelGen/Noise/SimplexF.h"
#include "World/Level/LevelGen/Feature/FeatureNoiseHolder.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <algorithm>
#include <limits>
#include <memory>

namespace {

    const BlockState &dripstoneBlockState() {
        static const BlockState state = VanillaBlocks::DRIPSTONE_BLOCK().toBlockState();
        return state;
    }

    const BlockState &pointedDripstoneState() {
        static const BlockState state = VanillaBlocks::POINTED_DRIPSTONE().toBlockState();
        return state;
    }

    const SimplexF *clusterNoise() {
        const FeatureNoiseHolder *holder = FeatureNoiseHolder::get();
        if (holder == nullptr)
            return nullptr;

        return &holder->getDripstoneCluster();
    }

    float remapFromNormalized(float input, float outMin, float outMax) {
        return outMin + ((input - -1.0f) / (1.0f - -1.0f) * (outMax - outMin));
    }

    double clampDouble(double value, double min, double max) {
        return value < min ? min : std::min(value, max);
    }

    const char *thicknessForIndex(int32_t index, int32_t lastIndex, bool merged) {
        if (index == lastIndex)
            return merged ? "merge" : "tip";

        if (index == 0 && lastIndex >= 2)
            return "base";

        if (index == lastIndex - 1)
            return "frustum";

        return "middle";
    }

    BlockState dripstoneStateOf(bool hanging, const char *thickness) {
        BlockState state = DecorationSupport::withByteState(pointedDripstoneState(), "hanging", hanging);
        return DecorationSupport::withState(state, "dripstone_thickness", thickness);
    }

}

const char *SpeleothemClusterFeature::name() const {
    return "minecraft:dripstone_cluster_feature";
}

std::vector<int32_t> SpeleothemClusterFeature::_getHighestWorkableBlocks(LevelChunk &chunk, int32_t x, int32_t z) {
    std::vector<int32_t> blockYs;

    for (int32_t y = chunk.getHeight(x, z); y > LevelChunk::MIN_Y; --y) {
        if ((int32_t) chunk.getBiomeAt(x, y, z) != BiomeIds::DRIPSTONE_CAVES)
            continue;

        const std::string &id = chunk.getBlock(x, y, z).mName;
        if ((id == "minecraft:stone" || id == "minecraft:deepslate")
            && DecorationSupport::isAir(chunk.getBlock(x, y + 1, z)))
            blockYs.push_back(y);
    }

    return blockYs;
}

std::vector<int32_t> SpeleothemClusterFeature::_getLowestWorkableBlocks(LevelChunk &chunk, int32_t x, int32_t z) {
    std::vector<int32_t> blockYs;

    for (int32_t y = chunk.getHeight(x, z); y > LevelChunk::MIN_Y; --y) {
        if ((int32_t) chunk.getBiomeAt(x, y, z) != BiomeIds::DRIPSTONE_CAVES)
            continue;

        const std::string &id = chunk.getBlock(x, y, z).mName;
        if ((id == "minecraft:stone" || id == "minecraft:deepslate")
            && DecorationSupport::isAir(chunk.getBlock(x, y - 1, z)))
            blockYs.push_back(y);
    }

    return blockYs;
}

void SpeleothemClusterFeature::_placePointedDripstone(BlockManager &manager, int32_t x, int32_t y, int32_t z,
                                                      bool hanging, int32_t maxLength) {
    std::vector<int32_t> plannedY;
    const int32_t step = hanging ? -1 : 1;
    int32_t mergeY = std::numeric_limits<int32_t>::min();

    for (int32_t i = 0; i < maxLength; i++) {
        const int32_t currentY = y + (i * step);
        const BlockState currentBlock = manager.getBlockAt(x, currentY, z);
        if (currentBlock.mName == "minecraft:pointed_dripstone") {
            const Tag *tag = currentBlock.mStates.get("hanging");
            const bool currentHanging = tag != nullptr && tag->asByte() != 0;
            if (currentHanging != hanging && !plannedY.empty())
                mergeY = currentY;

            break;
        }

        if (!DecorationSupport::isAir(currentBlock))
            break;

        plannedY.push_back(currentY);
    }

    if (plannedY.empty())
        return;

    const int32_t lastIndex = (int32_t) plannedY.size() - 1;
    const bool merged = mergeY != std::numeric_limits<int32_t>::min();

    for (int32_t i = 0; i < (int32_t) plannedY.size(); i++)
        manager.setBlockStateAt(x, plannedY[(size_t) i], z,
                                dripstoneStateOf(hanging, thicknessForIndex(i, lastIndex, merged)));

    if (merged)
        manager.setBlockStateAt(x, mergeY, z, dripstoneStateOf(!hanging, "merge"));
}

void SpeleothemClusterFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    mRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunkX, chunkZ));
    const SimplexF *noisePtr = clusterNoise();
    if (noisePtr == nullptr)
        return;

    const SimplexF &noise = *noisePtr;

    BlockManager manager(level);

    for (int32_t x = 0; x < 16; x++) {
        const int32_t baseX = (chunkX << 4) + x;
        for (int32_t z = 0; z < 16; z++) {
            const int32_t baseZ = (chunkZ << 4) + z;
            if (noise.noise2D((float) baseX * 1.25f, (float) baseZ * 1.25f, true) <= 0.1)
                continue;

            bool hasDripstoneCave = false;
            for (int32_t y = chunk.getHeight(x, z); y > LevelChunk::MIN_Y; y--) {
                if ((int32_t) chunk.getBiomeAt(x, y, z) == BiomeIds::DRIPSTONE_CAVES) {
                    hasDripstoneCave = true;
                    break;
                }
            }

            if (!hasDripstoneCave)
                continue;

            for (int32_t y: _getHighestWorkableBlocks(chunk, x, z)) {
                const int32_t depth = (int32_t) clampDouble(
                        (double) remapFromNormalized(noise.noise3D((float) baseX, (float) y, (float) baseZ, true),
                                                     1.0f, 3.0f),
                        1.0, 2.0);
                bool water = false;

                for (int32_t i = 0; i < depth; i++) {
                    if (i == 0 && mRandom.nextFloat() < 0.002f) {
                        bool air = false;
                        for (const DecorationSupport::FaceOffset &face: DecorationSupport::HORIZONTALS) {
                            if (DecorationSupport::isAir(manager.getBlockAt(baseX + face.mX, y + face.mY,
                                                                           baseZ + face.mZ))) {
                                air = true;
                                break;
                            }
                        }

                        if (!air) {
                            manager.setBlockStateAt(baseX, y, baseZ, DecorationSupport::waterState());
                            water = true;
                        }
                    } else {
                        manager.setBlockStateAt(baseX, y - i, baseZ, dripstoneBlockState());
                    }
                }

                if (!water && mRandom.nextFloat() < 0.1f)
                    _placePointedDripstone(manager, baseX, y + 1, baseZ, false, mRandom.nextInt(1, 3));
            }

            for (int32_t y: _getLowestWorkableBlocks(chunk, x, z)) {
                const int32_t depth = (int32_t) clampDouble(
                        (double) remapFromNormalized(noise.noise3D((float) baseX, (float) y, (float) baseZ, true),
                                                     1.0f, 3.0f),
                        1.0, 2.0);

                for (int32_t i = 0; i < depth; i++)
                    manager.setBlockStateAt(baseX, y + i, baseZ, dripstoneBlockState());

                if (mRandom.nextFloat() < 0.3f)
                    _placePointedDripstone(manager, baseX, y - 1, baseZ, true, mRandom.nextInt(1, 5));
            }
        }
    }

    queueObject(manager);
}
