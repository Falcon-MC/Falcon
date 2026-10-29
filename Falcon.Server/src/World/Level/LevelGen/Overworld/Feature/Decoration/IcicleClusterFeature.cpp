#include "World/Level/LevelGen/Overworld/Feature/Decoration/IcicleClusterFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/IceCavesSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <algorithm>
#include <cstdlib>

// Port of the 26.60 speleothem cluster feature (the class behind dripstone_cluster_feature) with the
// parameters registered for minecraft:icicle_cluster_feature.
namespace {
    const int32_t MIN_ATTEMPTS = 24;
    const int32_t EXTRA_ATTEMPTS = 40;
    const int32_t MIN_Y = -64;
    const int32_t HEIGHT_RANGE = 320;

    const int32_t SEARCH_RANGE = 32;
    const int32_t MIN_HEIGHT = 3;
    const int32_t MAX_HEIGHT = 5;
    const int32_t RADIUS = 6;
    const float MIN_DENSITY = 0.3f;
    const float MAX_DENSITY = 0.7f;
    const float EDGE_CHANCE_DISTANCE = 3.0f;
    const float MIN_COLUMN_CHANCE = 0.1f;
    const float CENTER_BIAS_FALLOFF = 0.125f;
    const float HEIGHT_DEVIATION = 3.0f;

    const BlockState &icicleBaseState() {
        static const BlockState state = VanillaBlocks::ICICLE().toBlockState();
        return state;
    }

    const char *thicknessOf(int32_t index, int32_t length) {
        const int32_t remaining = length - index;
        if (remaining == 1)
            return "tip";
        if (remaining == 2)
            return "frustum";
        return index == 0 ? "base" : "middle";
    }

    // The block touching the ceiling is the only one marked as attached.
    BlockState icicleState(int32_t index, int32_t length) {
        BlockState state = DecorationSupport::withByteState(icicleBaseState(), "hanging", true);
        state = DecorationSupport::withByteState(state, "attached_bit", index == 0);
        return DecorationSupport::withState(state, "dripstone_thickness", thicknessOf(index, length));
    }

    bool isEmpty(const BlockState &state) {
        return DecorationSupport::isAir(state);
    }

    bool isEndpoint(const BlockState &state) {
        return state == IceCavesSupport::packedIceState() || IceCavesSupport::isLava(state);
    }

    // Walks up from an empty origin and reports the first block that ends the open column, if it can
    // carry an icicle.
    bool findCeiling(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t &ceilingY) {
        int32_t steps = 0;
        int32_t currentY = y + 1;
        while (steps < SEARCH_RANGE - 1 && isEmpty(manager.getBlockAt(x, currentY, z))) {
            currentY++;
            steps++;
        }

        if (!isEndpoint(manager.getBlockAt(x, currentY, z)))
            return false;

        ceilingY = y + steps + 1;
        return true;
    }

    int32_t biasedLength(IRandom &random, int32_t maxLength, int32_t dx, int32_t dz) {
        const float spread = (float) maxLength * 0.5f;
        const float falloff = (float) (std::abs(dx) + std::abs(dz)) * CENTER_BIAS_FALLOFF;
        const float bias = falloff > 1.0f ? 0.0f : spread + (0.0f - spread) * falloff;
        const float length = std::max((float) random.nextGaussian() * HEIGHT_DEVIATION + bias, 0.0f);
        return (float) maxLength < length ? maxLength : (int32_t) length;
    }

    void growIcicle(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t length) {
        for (int32_t i = 0; i < length; i++) {
            const BlockState &current = manager.getBlockAt(x, y - i, z);
            if (!(DecorationSupport::isAir(current) || DecorationSupport::isWater(current)))
                return;

            manager.setBlockStateAt(x, y - i, z, icicleState(i, length));
        }
    }

    // icicle_cluster_feature only grows from the ceiling and has no wetness, so the stalagmite, merge and
    // water passes of the shared cluster code never run for it. Its base layer swaps packed ice for packed
    // ice, so it is skipped as well.
    void placeCluster(BlockManager &manager, IRandom &random, int32_t originX, int32_t originY, int32_t originZ) {
        if (!isEmpty(manager.getBlockAt(originX, originY, originZ)))
            return;

        const int32_t height = MIN_HEIGHT + random.nextInt(MAX_HEIGHT - MIN_HEIGHT + 1);
        const float density = MIN_DENSITY + (MAX_DENSITY - MIN_DENSITY) * random.nextFloat();

        for (int32_t dx = -RADIUS; dx <= RADIUS; dx++) {
            for (int32_t dz = -RADIUS; dz <= RADIUS; dz++) {
                const float edge = (float) std::min(RADIUS - std::abs(dx), RADIUS - std::abs(dz)) / EDGE_CHANCE_DISTANCE;
                const double chance = edge < 0.0f ? MIN_COLUMN_CHANCE
                                                  : edge > 1.0f ? 1.0 : MIN_COLUMN_CHANCE + (1.0f - MIN_COLUMN_CHANCE) * edge;

                const int32_t x = originX + dx;
                const int32_t z = originZ + dz;
                int32_t ceilingY = 0;
                if (!isEmpty(manager.getBlockAt(x, originY, z)) || !findCeiling(manager, x, originY, z, ceilingY))
                    continue;

                if (chance <= random.nextDouble() || IceCavesSupport::isLava(manager.getBlockAt(x, ceilingY, z)))
                    continue;

                int32_t length = 0;
                if (random.nextFloat() <= density)
                    length = biasedLength(random, height, dx, dz);

                int32_t open = 0;
                while (open < length && isEmpty(manager.getBlockAt(x, ceilingY - 1 - open, z)))
                    open++;

                growIcicle(manager, x, ceilingY - 1, z, open);
            }
        }
    }
}

const char *IcicleClusterFeature::name() const {
    return "minecraft:icicle_cluster_feature";
}

void IcicleClusterFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    mRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunk.getX(), chunk.getZ())
                    ^ javaStringHash(name()));

    BlockManager manager(level);
    manager.readPendingFrom(mRoot);
    const int32_t attempts = MIN_ATTEMPTS + mRandom.nextBoundedInt(EXTRA_ATTEMPTS);
    for (int32_t i = 0; i < attempts; i++) {
        const int32_t x = (chunk.getX() << 4) + mRandom.nextBoundedInt(15);
        const int32_t y = MIN_Y + mRandom.nextBoundedInt(HEIGHT_RANGE);
        const int32_t z = (chunk.getZ() << 4) + mRandom.nextBoundedInt(15);
        if (IceCavesSupport::isIceCaves(chunk, x, y, z))
            placeCluster(manager, mRandom, x, y, z);
    }

    queueObject(manager);
}
