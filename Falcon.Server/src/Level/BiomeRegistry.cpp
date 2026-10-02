#include "Level/BiomeRegistry.h"

BiomeRegistry::BiomeRegistry() {
    auto add = [this](const char *name) {
        BiomeDefinitionData biome;
        biome.mName = name;
        mBiomes.push_back(biome);
    };

    add("badlands");
    add("bamboo_jungle");
    add("basalt_deltas");
    add("beach");
    add("birch_forest");
    add("cherry_grove");
    add("cold_ocean");
    add("crimson_forest");
    add("dark_forest");
    add("deep_cold_ocean");
    add("deep_dark");
    add("deep_frozen_ocean");
    add("deep_lukewarm_ocean");
    add("deep_ocean");
    add("desert");
    add("dripstone_caves");
    add("end_barrens");
    add("end_highlands");
    add("end_midlands");
    add("eroded_badlands");
    add("flower_forest");
    add("forest");
    add("frozen_ocean");
    add("frozen_peaks");
    add("frozen_river");
    add("grove");
    add("ice_caves");
    add("ice_spikes");
    add("jagged_peaks");
    add("jungle");
    add("lukewarm_ocean");
    add("lush_caves");
    add("mangrove_swamp");
    add("meadow");
    add("mushroom_fields");
    add("nether_wastes");
    add("ocean");
    add("old_growth_birch_forest");
    add("old_growth_pine_taiga");
    add("old_growth_spruce_taiga");
    add("pale_garden");
    add("plains");
    add("river");
    add("savanna");
    add("savanna_plateau");
    add("small_end_islands");
    add("snowy_beach");
    add("snowy_plains");
    add("snowy_slopes");
    add("snowy_taiga");
    add("soul_sand_valley");
    add("sparse_jungle");
    add("stony_peaks");
    add("stony_shore");
    add("sunflower_plains");
    add("swamp");
    add("taiga");
    add("the_end");
    add("the_void");
    add("warm_ocean");
    add("warped_forest");
    add("windswept_forest");
    add("windswept_gravelly_hills");
    add("windswept_hills");
    add("windswept_savanna");
    add("wooded_badlands");
}

const std::vector<BiomeDefinitionData> &BiomeRegistry::getBiomes() const {
    return mBiomes;
}
