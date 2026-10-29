#include "World/Level/LevelGen/Overworld/Surface/SurfaceMaterialAttributes.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"

#include <unordered_map>

namespace {
    const BlockState &grassBlockState() {
        static const BlockState state = VanillaBlocks::GRASS_BLOCK().toBlockState();
        return state;
    }

    const BlockState &dirtState() {
        static const BlockState state = VanillaBlocks::DIRT().toBlockState();
        return state;
    }

    const BlockState &gravelState() {
        static const BlockState state = VanillaBlocks::GRAVEL().toBlockState();
        return state;
    }

    const BlockState &sandState() {
        static const BlockState state = VanillaBlocks::SAND().toBlockState();
        return state;
    }

    const BlockState &redSandState() {
        static const BlockState state = VanillaBlocks::RED_SAND().toBlockState();
        return state;
    }

    const BlockState &stoneState() {
        static const BlockState state = VanillaBlocks::STONE().toBlockState();
        return state;
    }

    const BlockState &snowState() {
        static const BlockState state = VanillaBlocks::SNOW().toBlockState();
        return state;
    }

    const BlockState &myceliumState() {
        static const BlockState state = VanillaBlocks::MYCELIUM().toBlockState();
        return state;
    }

    const BlockState &hardenedClayState() {
        static const BlockState state = VanillaBlocks::HARDENED_CLAY().toBlockState();
        return state;
    }

    const BlockState &coarseDirtState() {
        static const BlockState state = VanillaBlocks::COARSE_DIRT().toBlockState();
        return state;
    }

    const BlockState &podzolState() {
        static const BlockState state = VanillaBlocks::PODZOL().toBlockState();
        return state;
    }

    const BlockState &packedIceState() {
        static const BlockState state = VanillaBlocks::PACKED_ICE().toBlockState();
        return state;
    }

    const BlockState &iceState() {
        static const BlockState state = VanillaBlocks::ICE().toBlockState();
        return state;
    }

    const BlockState &powderSnowState() {
        static const BlockState state = VanillaBlocks::POWDER_SNOW().toBlockState();
        return state;
    }

    const BlockState &calciteState() {
        static const BlockState state = VanillaBlocks::CALCITE().toBlockState();
        return state;
    }

    const BlockState &mudState() {
        static const BlockState state = VanillaBlocks::MUD().toBlockState();
        return state;
    }

    using ComponentMap = std::unordered_map<int32_t, SurfaceBuilderComponent>;

    void addComponent(ComponentMap &components, int32_t biomeId, const BlockState &topBlock,
                      const BlockState &midBlock, const BlockState &seaFloorBlock, int32_t seaFloorDepth) {
        SurfaceBuilderComponent component;
        component.mSurfaceMaterial.mTopBlock = &topBlock;
        component.mSurfaceMaterial.mMidBlock = &midBlock;
        component.mSurfaceMaterial.mSeaFloorBlock = &seaFloorBlock;
        component.mSurfaceMaterial.mSeaFloorDepth = seaFloorDepth;
        components.emplace(biomeId, component);
    }

    void addAdjustment(ComponentMap &components, int32_t biomeId, float noiseLowerBound, float noiseUpperBound,
                       const BlockState *topBlock, const BlockState *midBlock, const BlockState *seaFloorBlock) {
        SurfaceMaterialAdjustmentAttributes adjustment;
        adjustment.mNoiseLowerBound = noiseLowerBound;
        adjustment.mNoiseUpperBound = noiseUpperBound;
        adjustment.mTopBlock = topBlock;
        adjustment.mMidBlock = midBlock;
        adjustment.mSeaFloorBlock = seaFloorBlock;
        components.at(biomeId).mSurfaceMaterialAdjustments.push_back(adjustment);
    }

    const ComponentMap &components() {
        static const ComponentMap map = [] {
            ComponentMap result;

            addComponent(result, BiomeIds::OCEAN, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::PLAINS, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::DESERT, sandState(), sandState(), gravelState(), 7);
            addComponent(result, BiomeIds::EXTREME_HILLS, grassBlockState(), dirtState(), gravelState(), 7);
            addAdjustment(result, BiomeIds::EXTREME_HILLS, 0.12099999934434891f, 1.0f,
                          &stoneState(), &stoneState(), nullptr);
            addComponent(result, BiomeIds::FOREST, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::TAIGA, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::SWAMPLAND, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::RIVER, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::FROZEN_RIVER, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::ICE_PLAINS, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::MUSHROOM_ISLAND, myceliumState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::BEACH, sandState(), sandState(), gravelState(), 7);
            addComponent(result, BiomeIds::JUNGLE, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::JUNGLE_EDGE, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::DEEP_OCEAN, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::STONE_BEACH, stoneState(), stoneState(), gravelState(), 7);
            addAdjustment(result, BiomeIds::STONE_BEACH, -0.05000000074505806f, 0.05000000074505806f,
                          &gravelState(), &gravelState(), nullptr);
            addComponent(result, BiomeIds::COLD_BEACH, sandState(), sandState(), gravelState(), 7);
            addComponent(result, BiomeIds::BIRCH_FOREST, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::ROOFED_FOREST, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::COLD_TAIGA, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::MEGA_TAIGA, grassBlockState(), dirtState(), gravelState(), 7);
            addAdjustment(result, BiomeIds::MEGA_TAIGA, 0.21199999749660492f, 1.0f,
                          &coarseDirtState(), nullptr, nullptr);
            addAdjustment(result, BiomeIds::MEGA_TAIGA, -0.11500000208616257f, 0.21199999749660492f,
                          &podzolState(), nullptr, nullptr);
            addComponent(result, BiomeIds::EXTREME_HILLS_PLUS_TREES, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::SAVANNA, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::SAVANNA_PLATEAU, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::MESA, redSandState(), hardenedClayState(), gravelState(), 7);
            addComponent(result, BiomeIds::MESA_PLATEAU_STONE, redSandState(), hardenedClayState(), gravelState(), 7);
            addComponent(result, BiomeIds::WARM_OCEAN, grassBlockState(), dirtState(), sandState(), 3);
            addComponent(result, BiomeIds::LUKEWARM_OCEAN, grassBlockState(), dirtState(), sandState(), 3);
            addComponent(result, BiomeIds::DEEP_LUKEWARM_OCEAN, grassBlockState(), dirtState(), sandState(), 7);
            addComponent(result, BiomeIds::COLD_OCEAN, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::DEEP_COLD_OCEAN, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::FROZEN_OCEAN, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::DEEP_FROZEN_OCEAN, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::BAMBOO_JUNGLE, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::SUNFLOWER_PLAINS, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::EXTREME_HILLS_MUTATED, grassBlockState(), dirtState(), gravelState(), 7);
            addAdjustment(result, BiomeIds::EXTREME_HILLS_MUTATED, -1.0f, -0.012000000104308128f,
                          &gravelState(), &gravelState(), nullptr);
            addAdjustment(result, BiomeIds::EXTREME_HILLS_MUTATED, 0.024000000208616257f, 1.0f,
                          &gravelState(), &gravelState(), nullptr);
            addComponent(result, BiomeIds::FLOWER_FOREST, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::ICE_PLAINS_SPIKES, snowState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::BIRCH_FOREST_MUTATED, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::REDWOOD_TAIGA_MUTATED, grassBlockState(), dirtState(), gravelState(), 7);
            addAdjustment(result, BiomeIds::REDWOOD_TAIGA_MUTATED, 0.21199999749660492f, 1.0f,
                          &coarseDirtState(), nullptr, nullptr);
            addAdjustment(result, BiomeIds::REDWOOD_TAIGA_MUTATED, -0.11500000208616257f, 0.21199999749660492f,
                          &podzolState(), nullptr, nullptr);
            addComponent(result, BiomeIds::SAVANNA_MUTATED, grassBlockState(), dirtState(), gravelState(), 7);
            addAdjustment(result, BiomeIds::SAVANNA_MUTATED, 0.21199999749660492f, 1.0f,
                          &stoneState(), &stoneState(), nullptr);
            addAdjustment(result, BiomeIds::SAVANNA_MUTATED, -0.061000000685453415f, 0.21199999749660492f,
                          &coarseDirtState(), nullptr, nullptr);
            addComponent(result, BiomeIds::MESA_BRYCE, redSandState(), hardenedClayState(), gravelState(), 7);
            addComponent(result, BiomeIds::JAGGED_PEAKS, snowState(), stoneState(), stoneState(), 7);
            addComponent(result, BiomeIds::FROZEN_PEAKS, snowState(), snowState(), iceState(), 7);
            addAdjustment(result, BiomeIds::FROZEN_PEAKS, -0.5f, 0.20000000298023224f,
                          nullptr, &packedIceState(), nullptr);
            addAdjustment(result, BiomeIds::FROZEN_PEAKS, 0.0f, 0.20000000298023224f,
                          &packedIceState(), nullptr, nullptr);
            addAdjustment(result, BiomeIds::FROZEN_PEAKS, -0.0625f, 0.02500000037252903f,
                          nullptr, &iceState(), nullptr);
            addAdjustment(result, BiomeIds::FROZEN_PEAKS, 0.0f, 0.02500000037252903f,
                          &iceState(), nullptr, nullptr);
            addComponent(result, BiomeIds::SNOWY_SLOPES, snowState(), snowState(), gravelState(), 7);
            addAdjustment(result, BiomeIds::SNOWY_SLOPES, 0.3499999940395355f, 0.6000000238418579f,
                          &powderSnowState(), nullptr, nullptr);
            addAdjustment(result, BiomeIds::SNOWY_SLOPES, 0.44999998807907104f, 0.5799999833106995f,
                          nullptr, &powderSnowState(), nullptr);
            addComponent(result, BiomeIds::GROVE, snowState(), dirtState(), gravelState(), 7);
            addAdjustment(result, BiomeIds::GROVE, 0.3499999940395355f, 0.6000000238418579f,
                          &powderSnowState(), nullptr, nullptr);
            addAdjustment(result, BiomeIds::GROVE, 0.44999998807907104f, 0.5799999833106995f,
                          nullptr, &powderSnowState(), nullptr);
            addComponent(result, BiomeIds::MEADOW, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::LUSH_CAVES, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::DRIPSTONE_CAVES, stoneState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::STONY_PEAKS, stoneState(), stoneState(), gravelState(), 7);
            addAdjustment(result, BiomeIds::STONY_PEAKS, -0.012500000186264515f, 0.012500000186264515f,
                          &calciteState(), &calciteState(), nullptr);
            addComponent(result, BiomeIds::DEEP_DARK, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::MANGROVE_SWAMP, mudState(), mudState(), mudState(), 7);
            addComponent(result, BiomeIds::CHERRY_GROVE, grassBlockState(), dirtState(), gravelState(), 7);
            addComponent(result, BiomeIds::PALE_GARDEN, grassBlockState(), dirtState(), gravelState(), 7);

            return result;
        }();

        return map;
    }
}

const SurfaceBuilderComponent *SurfaceBuilderComponent::getForBiome(int32_t biomeId) {
    const ComponentMap &map = components();
    const ComponentMap::const_iterator found = map.find(biomeId);
    if (found == map.end())
        return nullptr;

    return &found->second;
}
