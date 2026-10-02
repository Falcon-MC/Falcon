#include "Level/Generator/Overworld/Biome/ClimateAttributes.h"

#include "Level/Generator/Biome/BiomeIds.h"

#include <unordered_map>

namespace {
    using ClimateMap = std::unordered_map<int32_t, ClimateAttributes>;

    void add(ClimateMap &map, int32_t biomeId, float temperature, float downfall, bool rain) {
        ClimateAttributes attributes;
        attributes.mTemperature = temperature;
        attributes.mDownfall = downfall;
        attributes.mRain = rain;
        map.emplace(biomeId, attributes);
    }

    const ClimateMap &climates() {
        static const ClimateMap map = [] {
            ClimateMap result;

            add(result, BiomeIds::OCEAN, 0.5f, 0.5f, true);
            add(result, BiomeIds::PLAINS, 0.800000011920929f, 0.4000000059604645f, true);
            add(result, BiomeIds::DESERT, 2.0f, 0.0f, false);
            add(result, BiomeIds::EXTREME_HILLS, 0.20000000298023224f, 0.30000001192092896f, true);
            add(result, BiomeIds::FOREST, 0.699999988079071f, 0.800000011920929f, true);
            add(result, BiomeIds::TAIGA, 0.25f, 0.800000011920929f, true);
            add(result, BiomeIds::SWAMPLAND, 0.800000011920929f, 0.8999999761581421f, true);
            add(result, BiomeIds::RIVER, 0.5f, 0.5f, true);
            add(result, BiomeIds::LEGACY_FROZEN_OCEAN, 0.0f, 0.5f, true);
            add(result, BiomeIds::FROZEN_RIVER, 0.0f, 0.5f, true);
            add(result, BiomeIds::ICE_PLAINS, 0.0f, 0.5f, true);
            add(result, BiomeIds::MUSHROOM_ISLAND, 0.8999999761581421f, 1.0f, true);
            add(result, BiomeIds::BEACH, 0.800000011920929f, 0.4000000059604645f, true);
            add(result, BiomeIds::JUNGLE, 0.949999988079071f, 0.8999999761581421f, true);
            add(result, BiomeIds::JUNGLE_EDGE, 0.949999988079071f, 0.800000011920929f, true);
            add(result, BiomeIds::DEEP_OCEAN, 0.5f, 0.5f, true);
            add(result, BiomeIds::STONE_BEACH, 0.20000000298023224f, 0.30000001192092896f, true);
            add(result, BiomeIds::COLD_BEACH, 0.05000000074505806f, 0.30000001192092896f, true);
            add(result, BiomeIds::BIRCH_FOREST, 0.6000000238418579f, 0.6000000238418579f, true);
            add(result, BiomeIds::ROOFED_FOREST, 0.699999988079071f, 0.800000011920929f, true);
            add(result, BiomeIds::COLD_TAIGA, -0.5f, 0.4000000059604645f, true);
            add(result, BiomeIds::MEGA_TAIGA, 0.30000001192092896f, 0.800000011920929f, true);
            add(result, BiomeIds::EXTREME_HILLS_PLUS_TREES, 0.20000000298023224f, 0.30000001192092896f, true);
            add(result, BiomeIds::SAVANNA, 1.2000000476837158f, 0.0f, false);
            add(result, BiomeIds::SAVANNA_PLATEAU, 1.0f, 0.0f, false);
            add(result, BiomeIds::MESA, 2.0f, 0.0f, false);
            add(result, BiomeIds::MESA_PLATEAU_STONE, 2.0f, 0.0f, false);
            add(result, BiomeIds::WARM_OCEAN, 0.5f, 0.5f, true);
            add(result, BiomeIds::LUKEWARM_OCEAN, 0.5f, 0.5f, true);
            add(result, BiomeIds::DEEP_LUKEWARM_OCEAN, 0.5f, 0.5f, true);
            add(result, BiomeIds::COLD_OCEAN, 0.5f, 0.5f, true);
            add(result, BiomeIds::DEEP_COLD_OCEAN, 0.5f, 0.5f, true);
            add(result, BiomeIds::FROZEN_OCEAN, 0.0f, 0.5f, true);
            add(result, BiomeIds::DEEP_FROZEN_OCEAN, 0.5f, 0.5f, true);
            add(result, BiomeIds::BAMBOO_JUNGLE, 0.949999988079071f, 0.8999999761581421f, true);
            add(result, BiomeIds::SUNFLOWER_PLAINS, 0.800000011920929f, 0.4000000059604645f, true);
            add(result, BiomeIds::EXTREME_HILLS_MUTATED, 0.20000000298023224f, 0.30000001192092896f, true);
            add(result, BiomeIds::FLOWER_FOREST, 0.699999988079071f, 0.800000011920929f, true);
            add(result, BiomeIds::ICE_PLAINS_SPIKES, 0.0f, 1.0f, true);
            add(result, BiomeIds::BIRCH_FOREST_MUTATED, 0.6000000238418579f, 0.6000000238418579f, true);
            add(result, BiomeIds::REDWOOD_TAIGA_MUTATED, 0.25f, 0.800000011920929f, true);
            add(result, BiomeIds::SAVANNA_MUTATED, 2.0f, 0.0f, false);
            add(result, BiomeIds::MESA_BRYCE, 2.0f, 0.0f, false);
            add(result, BiomeIds::JAGGED_PEAKS, -0.699999988079071f, 0.8999999761581421f, true);
            add(result, BiomeIds::FROZEN_PEAKS, -0.699999988079071f, 0.8999999761581421f, true);
            add(result, BiomeIds::SNOWY_SLOPES, -0.30000001192092896f, 0.8999999761581421f, true);
            add(result, BiomeIds::GROVE, -0.20000000298023224f, 0.800000011920929f, true);
            add(result, BiomeIds::MEADOW, 0.30000001192092896f, 0.800000011920929f, true);
            add(result, BiomeIds::LUSH_CAVES, 0.8999999761581421f, 0.0f, false);
            add(result, BiomeIds::DRIPSTONE_CAVES, 0.20000000298023224f, 0.0f, false);
            add(result, BiomeIds::STONY_PEAKS, 1.0f, 0.30000001192092896f, true);
            add(result, BiomeIds::DEEP_DARK, 0.800000011920929f, 0.4000000059604645f, true);
            add(result, BiomeIds::MANGROVE_SWAMP, 0.800000011920929f, 0.8999999761581421f, true);
            add(result, BiomeIds::CHERRY_GROVE, 0.30000001192092896f, 0.800000011920929f, true);
            add(result, BiomeIds::PALE_GARDEN, 0.699999988079071f, 0.800000011920929f, true);
            add(result, BiomeIds::SULFUR_CAVES, 0.800000011920929f, 0.4000000059604645f, true);
            add(result, BiomeIds::ICE_CAVES, 0.0f, 0.0f, false);

            return result;
        }();

        return map;
    }
}

const ClimateAttributes *ClimateAttributes::getForBiome(int32_t biomeId) {
    const ClimateMap &map = climates();
    const ClimateMap::const_iterator found = map.find(biomeId);
    if (found == map.end())
        return nullptr;

    return &found->second;
}
