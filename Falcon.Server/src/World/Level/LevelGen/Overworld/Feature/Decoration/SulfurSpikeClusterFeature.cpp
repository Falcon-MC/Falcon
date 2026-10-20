#include "World/Level/LevelGen/Overworld/Feature/Decoration/SulfurSpikeClusterFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <algorithm>
#include <limits>
#include <vector>

namespace {

    const BlockState &sulfurSpikeState() {
        static const BlockState state = VanillaBlocks::SULFUR_SPIKE().toBlockState();
        return state;
    }

    bool isBaseBlock(const BlockState &state) {
        return state.mName == "minecraft:stone"
               || state.mName == "minecraft:deepslate"
               || state.mName == "minecraft:tuff"
               || state.mName == "minecraft:sulfur"
               || state.mName == "minecraft:cinnabar";
    }

    bool isAirOrWater(const BlockState &state) {
        return DecorationSupport::isAir(state)
               || DecorationSupport::isWater(state)
               || state.mName == "minecraft:sulfur_spike";
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

const char *SulfurSpikeClusterFeature::name() const {
    return "minecraft:sulfur_spike_cluster_feature";
}

int32_t SulfurSpikeClusterFeature::_nextRangeSafe(int32_t min, int32_t max) {
    if (max <= min)
        return min;

    return mRandom.nextInt(min, max);
}

void SulfurSpikeClusterFeature::_placeSulfurSpike(BlockManager &manager, int32_t x, int32_t y, int32_t z,
                                                  bool hanging, int32_t maxLength) {
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

void SulfurSpikeClusterFeature::_tryPlaceClusterSpike(BlockManager &manager, int32_t x, int32_t y, int32_t z,
                                                      bool hanging, int32_t maxSpikeHeight) {
    const int32_t step = hanging ? 1 : -1;

    for (int32_t i = 0; i <= 12; i++) {
        const int32_t supportY = y + (i * step);
        const BlockState support = manager.getBlockAt(x, supportY, z);
        if (isBaseBlock(support)) {
            _placeSulfurSpike(manager, x, hanging ? supportY - 1 : supportY + 1, z, hanging,
                              _nextRangeSafe(1, maxSpikeHeight));
            return;
        }

        if (!isAirOrWater(support))
            return;
    }
}

void SulfurSpikeClusterFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();

    mRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunk.getX(), chunk.getZ())
                    ^ javaStringHash(name()));

    BlockManager manager(level);
    const int32_t minY = LevelChunk::MIN_Y;
    const int32_t maxY = std::min(256, (int32_t) LevelChunk::MAX_Y);
    const int32_t attempts = mRandom.nextInt(48, 96);

    for (int32_t i = 0; i < attempts; i++) {
        const int32_t centerX = (chunk.getX() << 4) + mRandom.nextBoundedInt(15);
        const int32_t centerZ = (chunk.getZ() << 4) + mRandom.nextBoundedInt(15);
        const int32_t centerY = mRandom.nextInt(minY, maxY);

        if (DecorationSupport::getBiomeAt(level, centerX, centerY, centerZ) != BiomeIds::SULFUR_CAVES)
            continue;

        const int32_t radius = mRandom.nextInt(2, 8);
        const float density = 0.3f + mRandom.nextFloat() * 0.4f;
        const int32_t maxSpikeHeight = mRandom.nextInt(1, 4);

        for (int32_t dx = -radius; dx <= radius; dx++) {
            for (int32_t dz = -radius; dz <= radius; dz++) {
                if ((dx * dx) + (dz * dz) > radius * radius || mRandom.nextFloat() > density)
                    continue;

                const int32_t x = centerX + dx;
                const int32_t z = centerZ + dz;
                if ((x >> 4) != chunk.getX() || (z >> 4) != chunk.getZ())
                    continue;

                if (mRandom.nextBoolean())
                    _tryPlaceClusterSpike(manager, x, centerY, z, false, maxSpikeHeight);
                else
                    _tryPlaceClusterSpike(manager, x, centerY, z, true, maxSpikeHeight);
            }
        }
    }

    queueObject(manager);
}
