#include "World/Level/LevelGen/Structure/StructureMaterials.h"

#include <string>
#include <unordered_set>

namespace {
    // material types 5, 6 and 18: the three with flag 2 in the table at 0x14F1ABBD8 (26.60)
    const std::unordered_set<std::string> &liquids() {
        static const std::unordered_set<std::string> names = {
            "minecraft:water",
            "minecraft:flowing_water",
            "minecraft:lava",
            "minecraft:flowing_lava",
            "minecraft:bubble_column",
        };
        return names;
    }

    // material type 8: every block the game registers with it (sub_14CE9AD30, 26.60)
    const std::unordered_set<std::string> &plants() {
        static const std::unordered_set<std::string> names = {
            "minecraft:acacia_sapling",
            "minecraft:allium",
            "minecraft:azure_bluet",
            "minecraft:bamboo_sapling",
            "minecraft:beetroot",
            "minecraft:big_dripleaf",
            "minecraft:birch_sapling",
            "minecraft:blue_orchid",
            "minecraft:brown_mushroom",
            "minecraft:bush",
            "minecraft:cactus_flower",
            "minecraft:carrots",
            "minecraft:cave_vines",
            "minecraft:cave_vines_body_with_berries",
            "minecraft:cave_vines_head_with_berries",
            "minecraft:cherry_sapling",
            "minecraft:closed_eyeblossom",
            "minecraft:cocoa",
            "minecraft:cornflower",
            "minecraft:crimson_fungus",
            "minecraft:crimson_roots",
            "minecraft:dandelion",
            "minecraft:dark_oak_sapling",
            "minecraft:deadbush",
            "minecraft:fern",
            "minecraft:firefly_bush",
            "minecraft:glow_lichen",
            "minecraft:golden_dandelion",
            "minecraft:hanging_roots",
            "minecraft:jungle_sapling",
            "minecraft:kelp",
            "minecraft:large_fern",
            "minecraft:leaf_litter",
            "minecraft:lilac",
            "minecraft:lily_of_the_valley",
            "minecraft:mangrove_propagule",
            "minecraft:melon_stem",
            "minecraft:moss_carpet",
            "minecraft:nether_sprouts",
            "minecraft:nether_wart",
            "minecraft:oak_sapling",
            "minecraft:open_eyeblossom",
            "minecraft:orange_tulip",
            "minecraft:oxeye_daisy",
            "minecraft:pale_hanging_moss",
            "minecraft:pale_moss_carpet",
            "minecraft:pale_oak_sapling",
            "minecraft:peony",
            "minecraft:pink_petals",
            "minecraft:pink_tulip",
            "minecraft:pitcher_crop",
            "minecraft:pitcher_plant",
            "minecraft:poplar_sapling",
            "minecraft:poppy",
            "minecraft:potatoes",
            "minecraft:pumpkin_stem",
            "minecraft:red_mushroom",
            "minecraft:red_tulip",
            "minecraft:reeds",
            "minecraft:rose_bush",
            "minecraft:sea_pickle",
            "minecraft:seagrass",
            "minecraft:short_dry_grass",
            "minecraft:short_grass",
            "minecraft:small_dripleaf_block",
            "minecraft:spore_blossom",
            "minecraft:spruce_sapling",
            "minecraft:sunflower",
            "minecraft:sweet_berry_bush",
            "minecraft:tall_dry_grass",
            "minecraft:tall_grass",
            "minecraft:torchflower",
            "minecraft:torchflower_crop",
            "minecraft:twisting_vines",
            "minecraft:vine",
            "minecraft:warped_fungus",
            "minecraft:warped_roots",
            "minecraft:weeping_vines",
            "minecraft:wheat",
            "minecraft:white_tulip",
            "minecraft:wildflowers",
            "minecraft:wither_rose",
        };
        return names;
    }
}

bool StructureMaterials::isLiquid(const BlockState &state) {
    return liquids().count(state.mName) != 0;
}

bool StructureMaterials::isPlant(const BlockState &state) {
    return plants().count(state.mName) != 0;
}
