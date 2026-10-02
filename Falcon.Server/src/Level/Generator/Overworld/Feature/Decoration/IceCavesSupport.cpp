#include "Level/Generator/Overworld/Feature/Decoration/IceCavesSupport.h"

#include "Block/Blocks/VanillaBlocks.h"
#include "Level/Generator/Biome/BiomeIds.h"
#include "Level/Generator/Feature/BlockManager.h"
#include "Level/Generator/Feature/FeatureNoiseHolder.h"
#include "Level/Generator/Noise/SimplexNoise.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Generator/Random/IRandom.h"
#include "Level/LevelChunk.h"

#include <vector>

namespace {

    const BlockState &calciteState() {
        static const BlockState state = VanillaBlocks::CALCITE().toBlockState();
        return state;
    }

    const BlockState &icicleBaseState() {
        static const BlockState state = VanillaBlocks::ICICLE().toBlockState();
        return state;
    }

    const char *thicknessForIndex(int32_t index, int32_t lastIndex) {
        if (index == lastIndex)
            return "tip";

        if (index == 0 && lastIndex >= 2)
            return "base";

        if (index == lastIndex - 1)
            return "frustum";

        return "middle";
    }

}

namespace IceCavesSupport {

    const SimplexNoise *gradientNoise() {
        const FeatureNoiseHolder *holder = FeatureNoiseHolder::get();
        if (holder == nullptr)
            return nullptr;

        return &holder->getIceCaveGradient();
    }

    const BlockState *gradientState(float noise) {
        if (noise >= 0.5f && noise < 0.7f)
            return &calciteState();

        if (noise >= 0.3f && noise < 0.5f)
            return &packedIceState();

        if (noise >= 0.2f && noise < 0.3f)
            return &calciteState();

        if (noise >= -0.5f && noise < 0.2f)
            return &packedIceState();

        if (noise >= -0.6f && noise < -0.5f)
            return &calciteState();

        return nullptr;
    }

    const BlockState &packedIceState() {
        static const BlockState state = VanillaBlocks::PACKED_ICE().toBlockState();
        return state;
    }

    const BlockState &iceCrystalState() {
        static const BlockState state = DecorationSupport::withState(VanillaBlocks::ICE_CRYSTAL().toBlockState(),
                                                                     "minecraft:block_face", "up");
        return state;
    }

    const BlockState &snowLayerState() {
        static const BlockState state = VanillaBlocks::SNOW_LAYER().toBlockState();
        return state;
    }

    BlockState icicleState(int32_t index, int32_t lastIndex) {
        const BlockState state = DecorationSupport::withByteState(icicleBaseState(), "hanging", true);
        return DecorationSupport::withState(state, "dripstone_thickness", thicknessForIndex(index, lastIndex));
    }

    bool isNaturalBaseBlock(const BlockState &state) {
        return state.mName == "minecraft:stone" || state.mName == "minecraft:deepslate"
               || state.mName == "minecraft:tuff";
    }

    bool isIceCaves(LevelChunk &chunk, int32_t x, int32_t y, int32_t z) {
        if ((x >> 4) != chunk.getX() || (z >> 4) != chunk.getZ() || y <= LevelChunk::MIN_Y
            || y >= LevelChunk::MAX_Y)
            return false;

        return (int32_t) chunk.getBiomeAt(x & 0x0f, y, z & 0x0f) == BiomeIds::ICE_CAVES;
    }

    int32_t triangle(IRandom &random, int32_t min, int32_t max) {
        const int32_t range = max - min;
        const int32_t lower = range / 2;
        const int32_t upper = range - lower;
        return min + random.nextBoundedInt(upper) + random.nextBoundedInt(lower);
    }

    bool findFloor(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t range, int32_t &floorY) {
        for (int32_t i = 0; i <= range; i++) {
            const int32_t currentY = y - i;
            if (currentY <= LevelChunk::MIN_Y)
                return false;

            if (!DecorationSupport::isAir(manager.getBlockAt(x, currentY, z)))
                continue;

            if (DecorationSupport::isSolid(manager.getBlockAt(x, currentY - 1, z))) {
                floorY = currentY - 1;
                return true;
            }
        }
        return false;
    }

    bool findCeiling(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t range, int32_t &ceilingY) {
        for (int32_t i = 0; i <= range; i++) {
            const int32_t currentY = y + i;
            if (currentY >= LevelChunk::MAX_Y)
                return false;

            if (!DecorationSupport::isAir(manager.getBlockAt(x, currentY, z)))
                continue;

            const BlockState &above = manager.getBlockAt(x, currentY + 1, z);
            if (above.getHash() == packedIceState().getHash()) {
                ceilingY = currentY + 1;
                return true;
            }
        }
        return false;
    }

    void placeIcicle(BlockManager &manager, int32_t x, int32_t ceilingY, int32_t z, int32_t maxLength) {
        std::vector<int32_t> plannedY;
        for (int32_t i = 1; i <= maxLength; i++) {
            const int32_t currentY = ceilingY - i;
            if (!DecorationSupport::isAir(manager.getBlockAt(x, currentY, z)))
                break;

            plannedY.push_back(currentY);
        }

        const int32_t lastIndex = (int32_t) plannedY.size() - 1;
        for (int32_t i = 0; i <= lastIndex; i++)
            manager.setBlockStateAt(x, plannedY[(size_t) i], z, icicleState(i, lastIndex));
    }

}
