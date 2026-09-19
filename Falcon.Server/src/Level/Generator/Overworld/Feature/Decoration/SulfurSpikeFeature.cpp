#include "Level/Generator/Overworld/Feature/Decoration/SulfurSpikeFeature.h"

#include "Block/Blocks/VanillaBlocks.h"
#include "Core/Math/MathConstants.h"
#include "Level/Generator/Biome/BiomeIds.h"
#include "Level/Generator/Feature/BlockManager.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Generator/Noise/SimplexNoise.h"
#include "Level/Generator/Feature/FeatureNoiseHolder.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

namespace {

    const BlockState &cinnabarState() {
        static const BlockState state = VanillaBlocks::CINNABAR().toBlockState();
        return state;
    }

    const BlockState &sulfurState() {
        static const BlockState state = VanillaBlocks::SULFUR().toBlockState();
        return state;
    }

    const BlockState &sulfurSpikeState() {
        static const BlockState state = VanillaBlocks::SULFUR_SPIKE().toBlockState();
        return state;
    }

    const SimplexNoise *gradientNoise() {
        const FeatureNoiseHolder *holder = FeatureNoiseHolder::get();
        if (holder == nullptr)
            return nullptr;

        return &holder->getSulfurCaveGradient();
    }

    bool isNaturalBaseBlock(const BlockState &state) {
        return state.mName == "minecraft:stone"
               || state.mName == "minecraft:deepslate"
               || state.mName == "minecraft:tuff";
    }

    bool isBaseBlock(const BlockState &state) {
        return isNaturalBaseBlock(state)
               || state.mName == "minecraft:sulfur"
               || state.mName == "minecraft:cinnabar";
    }

    bool isAirOrWater(const BlockState &state) {
        return DecorationSupport::isAir(state) || DecorationSupport::isWater(state);
    }

    const BlockState *gradientSurfaceState(float noise) {
        if ((noise >= -0.4f && noise < -0.1f) || (noise >= 0.4f && noise <= 1.0f))
            return &cinnabarState();

        if (noise >= 0.0f && noise < 0.4f)
            return &sulfurState();

        return nullptr;
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

    BlockState spikeStateOf(bool hanging, const char *thickness) {
        BlockState state = DecorationSupport::withByteState(sulfurSpikeState(), "hanging", hanging);
        return DecorationSupport::withState(state, "dripstone_thickness", thickness);
    }

}

const char *SulfurSpikeFeature::name() const {
    return "minecraft:sulfur_spike_feature";
}

int32_t SulfurSpikeFeature::_clampedNormal(float deviation, int32_t max) {
    const double u1 = std::max(std::numeric_limits<double>::min(), mRandom.nextDouble());
    const double u2 = mRandom.nextDouble();
    const int32_t value = (int32_t) std::lround(
            (float) (std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * MathConstants::PI * u2) * (double) deviation));

    if (value < -max)
        return -max;

    return std::min(value, max);
}

void SulfurSpikeFeature::_placeSulfurSpike(BlockManager &manager, int32_t x, int32_t y, int32_t z, bool hanging,
                                           int32_t maxLength) {
    std::vector<int32_t> plannedY;
    const int32_t step = hanging ? -1 : 1;
    int32_t mergeY = std::numeric_limits<int32_t>::min();

    for (int32_t i = 0; i < maxLength; i++) {
        const int32_t currentY = y + (i * step);
        const BlockState currentBlock = manager.getBlockAt(x, currentY, z);
        if (currentBlock.mName == "minecraft:sulfur_spike") {
            const Tag *tag = currentBlock.mStates.get("hanging");
            const bool currentHanging = tag != nullptr && tag->asByte() != 0;
            if (currentHanging != hanging && !plannedY.empty())
                mergeY = currentY;

            break;
        }

        if (!isAirOrWater(currentBlock))
            break;

        plannedY.push_back(currentY);
    }

    if (plannedY.empty())
        return;

    const int32_t lastIndex = (int32_t) plannedY.size() - 1;
    const bool merged = mergeY != std::numeric_limits<int32_t>::min();

    for (int32_t i = 0; i < (int32_t) plannedY.size(); i++)
        manager.setBlockStateAt(x, plannedY[(size_t) i], z,
                                spikeStateOf(hanging, thicknessForIndex(i, lastIndex, merged)));

    if (merged)
        manager.setBlockStateAt(x, mergeY, z, spikeStateOf(!hanging, "merge"));
}

void SulfurSpikeFeature::_applyNoiseGradientSurface(LevelChunk &chunk, BlockManager &manager,
                                                    const SimplexNoise &gradient) {
    for (int32_t localX = 0; localX < 16; localX++) {
        const int32_t worldX = (chunk.getX() << 4) + localX;

        for (int32_t localZ = 0; localZ < 16; localZ++) {
            const int32_t worldZ = (chunk.getZ() << 4) + localZ;

            for (int32_t y = std::min(256, chunk.getHeight(localX, localZ)); y > LevelChunk::MIN_Y + 1; y--) {
                if ((int32_t) chunk.getBiomeAt(localX, y, localZ) != BiomeIds::SULFUR_CAVES)
                    continue;

                const BlockState &state = chunk.getBlock(localX, y, localZ);
                if (!isNaturalBaseBlock(state))
                    continue;

                const bool exposed = isAirOrWater(manager.getBlockAt(worldX, y + 1, worldZ))
                                     || isAirOrWater(manager.getBlockAt(worldX, y - 1, worldZ))
                                     || isAirOrWater(manager.getBlockAt(worldX + 1, y, worldZ))
                                     || isAirOrWater(manager.getBlockAt(worldX - 1, y, worldZ))
                                     || isAirOrWater(manager.getBlockAt(worldX, y, worldZ + 1))
                                     || isAirOrWater(manager.getBlockAt(worldX, y, worldZ - 1));
                if (!exposed)
                    continue;

                const BlockState *gradientState = gradientSurfaceState(
                        gradient.getValue((double) worldX, (double) y, (double) worldZ));
                if (gradientState != nullptr)
                    manager.setBlockStateAt(worldX, y, worldZ, *gradientState);
            }
        }
    }
}

void SulfurSpikeFeature::_tryPlaceFromFloorScan(BlockManager &manager, const SimplexNoise &gradient, int32_t x,
                                                int32_t y, int32_t z) {
    for (int32_t i = 0; i <= 12; i++) {
        const int32_t currentY = y - i;
        const BlockState currentBlock = manager.getBlockAt(x, currentY, z);
        if (isBaseBlock(currentBlock)) {
            const BlockState *gradientState = gradientSurfaceState(
                    gradient.getValue((double) x, (double) currentY, (double) z));
            manager.setBlockStateAt(x, currentY, z, gradientState == nullptr ? sulfurState() : *gradientState);
            _placeSulfurSpike(manager, x, currentY + 1, z, false, mRandom.nextInt(1, 5));
            return;
        }

        if (!isAirOrWater(currentBlock))
            return;
    }
}

void SulfurSpikeFeature::_tryPlaceFromCeilingScan(BlockManager &manager, const SimplexNoise &gradient, int32_t x,
                                                  int32_t y, int32_t z) {
    for (int32_t i = 0; i <= 12; i++) {
        const int32_t currentY = y + i;
        const BlockState currentBlock = manager.getBlockAt(x, currentY, z);
        if (isBaseBlock(currentBlock)) {
            const BlockState *gradientState = gradientSurfaceState(
                    gradient.getValue((double) x, (double) currentY, (double) z));
            manager.setBlockStateAt(x, currentY, z, gradientState == nullptr ? sulfurState() : *gradientState);
            _placeSulfurSpike(manager, x, currentY - 1, z, true, mRandom.nextInt(1, 5));
            return;
        }

        if (!isAirOrWater(currentBlock))
            return;
    }
}

void SulfurSpikeFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();

    mRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunk.getX(), chunk.getZ())
                    ^ javaStringHash(name()));

    BlockManager manager(level);
    const SimplexNoise *gradientPtr = gradientNoise();
    if (gradientPtr != nullptr)
        _applyNoiseGradientSurface(chunk, manager, *gradientPtr);

    const int32_t minY = LevelChunk::MIN_Y;
    const int32_t maxY = std::min(256, (int32_t) LevelChunk::MAX_Y);
    const int32_t attempts = mRandom.nextInt(192, 256);

    for (int32_t i = 0; i < attempts; i++) {
        const int32_t baseX = (chunk.getX() << 4) + mRandom.nextBoundedInt(15);
        const int32_t baseZ = (chunk.getZ() << 4) + mRandom.nextBoundedInt(15);
        const int32_t baseY = mRandom.nextInt(minY, maxY);
        const int32_t innerCount = mRandom.nextInt(1, 5);

        for (int32_t j = 0; j < innerCount; j++) {
            const int32_t x = baseX + _clampedNormal(3.0f, 10);
            const int32_t y = baseY + _clampedNormal(0.6f, 2);
            const int32_t z = baseZ + _clampedNormal(3.0f, 10);

            if ((x >> 4) != chunk.getX() || (z >> 4) != chunk.getZ() || y < minY || y > maxY
                || (int32_t) chunk.getBiomeAt(x & 0x0f, y, z & 0x0f) != BiomeIds::SULFUR_CAVES)
                continue;

            const bool fromFloor = mRandom.nextBoolean();
            if (gradientPtr == nullptr)
                continue;

            if (fromFloor)
                _tryPlaceFromFloorScan(manager, *gradientPtr, x, y, z);
            else
                _tryPlaceFromCeilingScan(manager, *gradientPtr, x, y, z);
        }
    }

    queueObject(manager);
}
