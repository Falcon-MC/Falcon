#include "World/Level/Block/BlockPrecipitation.h"

#include <string>
#include <unordered_set>

namespace {
    // obstruct_rain (ice, packed_ice, grass_path) is left out: it stops rain like a block without an interaction
    const std::unordered_set<std::string> &noneBlocks() {
        static const std::unordered_set<std::string> blocks = {
                "minecraft:air", "minecraft:golden_rail", "minecraft:detector_rail", "minecraft:web",
                "minecraft:deadbush", "minecraft:redstone_wire", "minecraft:wheat", "minecraft:ladder",
                "minecraft:rail", "minecraft:lever", "minecraft:unlit_redstone_torch", "minecraft:redstone_torch",
                "minecraft:torch", "minecraft:snow_layer", "minecraft:reeds", "minecraft:portal",
                "minecraft:unpowered_repeater", "minecraft:powered_repeater", "minecraft:pumpkin_stem",
                "minecraft:melon_stem", "minecraft:vine", "minecraft:waterlily", "minecraft:nether_wart",
                "minecraft:soul_fire", "minecraft:end_portal", "minecraft:activator_rail", "minecraft:cocoa",
                "minecraft:tripwire_hook", "minecraft:trip_wire", "minecraft:flower_pot", "minecraft:carrots",
                "minecraft:potatoes", "minecraft:unpowered_comparator", "minecraft:powered_comparator",
                "minecraft:frame", "minecraft:end_rod", "minecraft:end_gateway", "minecraft:structure_void",
                "minecraft:beetroot", "minecraft:fire", "minecraft:underwater_torch", "minecraft:colored_torch_red",
                "minecraft:colored_torch_blue", "minecraft:colored_torch_green", "minecraft:colored_torch_purple",
                "minecraft:kelp", "minecraft:seagrass", "minecraft:sea_pickle", "minecraft:conduit",
                "minecraft:bubble_column", "minecraft:barrier", "minecraft:scaffolding", "minecraft:bamboo",
                "minecraft:bamboo_sapling", "minecraft:sweet_berry_bush", "minecraft:campfire",
                "minecraft:weeping_vines", "minecraft:soul_torch", "minecraft:twisting_vines",
                "minecraft:soul_campfire", "minecraft:azalea", "minecraft:flowering_azalea",
                "minecraft:hanging_roots", "minecraft:spore_blossom", "minecraft:cave_vines",
                "minecraft:cave_vines_body_with_berries", "minecraft:cave_vines_head_with_berries",
                "minecraft:big_dripleaf", "minecraft:glow_frame", "minecraft:moss_carpet",
                "minecraft:small_dripleaf_block", "minecraft:glow_lichen", "minecraft:candle",
                "minecraft:white_candle", "minecraft:orange_candle", "minecraft:magenta_candle",
                "minecraft:light_blue_candle", "minecraft:yellow_candle", "minecraft:lime_candle",
                "minecraft:pink_candle", "minecraft:gray_candle", "minecraft:light_gray_candle",
                "minecraft:cyan_candle", "minecraft:purple_candle", "minecraft:blue_candle",
                "minecraft:brown_candle", "minecraft:green_candle", "minecraft:red_candle",
                "minecraft:black_candle", "minecraft:sculk_vein", "minecraft:frog_spawn",
                "minecraft:mangrove_propagule", "minecraft:cherry_sapling", "minecraft:pink_petals",
                "minecraft:torchflower_crop", "minecraft:decorated_pot", "minecraft:pitcher_crop",
                "minecraft:pitcher_plant", "minecraft:white_carpet", "minecraft:orange_carpet",
                "minecraft:magenta_carpet", "minecraft:light_blue_carpet", "minecraft:yellow_carpet",
                "minecraft:lime_carpet", "minecraft:pink_carpet", "minecraft:gray_carpet",
                "minecraft:light_gray_carpet", "minecraft:cyan_carpet", "minecraft:purple_carpet",
                "minecraft:blue_carpet", "minecraft:brown_carpet", "minecraft:green_carpet", "minecraft:red_carpet",
                "minecraft:black_carpet", "minecraft:oak_sapling", "minecraft:spruce_sapling",
                "minecraft:birch_sapling", "minecraft:jungle_sapling", "minecraft:acacia_sapling",
                "minecraft:dark_oak_sapling", "minecraft:sunflower", "minecraft:lilac", "minecraft:tall_grass",
                "minecraft:large_fern", "minecraft:rose_bush", "minecraft:peony", "minecraft:light_block_0",
                "minecraft:light_block_1", "minecraft:light_block_2", "minecraft:light_block_3",
                "minecraft:light_block_4", "minecraft:light_block_5", "minecraft:light_block_6",
                "minecraft:light_block_7", "minecraft:light_block_8", "minecraft:light_block_9",
                "minecraft:light_block_10", "minecraft:light_block_11", "minecraft:light_block_12",
                "minecraft:light_block_13", "minecraft:light_block_14", "minecraft:light_block_15",
                "minecraft:pale_oak_sapling", "minecraft:pale_hanging_moss", "minecraft:pale_moss_carpet",
                "minecraft:resin_clump", "minecraft:skeleton_skull", "minecraft:wither_skeleton_skull",
                "minecraft:zombie_head", "minecraft:player_head", "minecraft:creeper_head", "minecraft:dragon_head",
                "minecraft:piglin_head", "minecraft:wildflowers", "minecraft:leaf_litter",
                "minecraft:cactus_flower", "minecraft:copper_torch", "minecraft:poplar_sapling"
        };
        return blocks;
    }

    const std::unordered_set<std::string> &snowloggingBlocks() {
        static const std::unordered_set<std::string> blocks = {
                "minecraft:dandelion", "minecraft:brown_mushroom", "minecraft:red_mushroom", "minecraft:wither_rose",
                "minecraft:crimson_roots", "minecraft:warped_roots", "minecraft:crimson_fungus",
                "minecraft:warped_fungus", "minecraft:nether_sprouts", "minecraft:torchflower", "minecraft:poppy",
                "minecraft:blue_orchid", "minecraft:allium", "minecraft:azure_bluet", "minecraft:red_tulip",
                "minecraft:orange_tulip", "minecraft:white_tulip", "minecraft:pink_tulip", "minecraft:oxeye_daisy",
                "minecraft:cornflower", "minecraft:lily_of_the_valley", "minecraft:short_grass", "minecraft:fern",
                "minecraft:open_eyeblossom", "minecraft:closed_eyeblossom", "minecraft:bush",
                "minecraft:firefly_bush", "minecraft:short_dry_grass", "minecraft:tall_dry_grass",
                "minecraft:golden_dandelion"
        };
        return blocks;
    }
}

bool BlockPrecipitation::letsRainThrough(const BlockState &state) {
    return noneBlocks().count(state.mName) != 0 || isSnowloggable(state);
}

bool BlockPrecipitation::isSnowloggable(const BlockState &state) {
    return snowloggingBlocks().count(state.mName) != 0;
}
