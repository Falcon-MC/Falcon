#include "World/Level/LevelGen/Overworld/Feature/Decoration/IceCavesSupport.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Feature/FeatureNoiseHolder.h"
#include "World/Level/LevelGen/Feature/IFeature.h"
#include "World/Level/LevelGen/Noise/SimplexNoise.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/LevelGen/Random/IRandom.h"
#include "World/Level/Chunk/LevelChunk.h"


namespace {

    const BlockState &calciteState() {
        static const BlockState state = VanillaBlocks::CALCITE().toBlockState();
        return state;
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

    bool isLava(const BlockState &state) {
        return DecorationSupport::isLiquid(state) && !DecorationSupport::isWater(state);
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
        return min + IFeature::randomRange(random, 0, upper) + IFeature::randomRange(random, 0, lower);
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

}
