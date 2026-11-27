#include "World/Level/LevelGen/Structure/JigsawStructures.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {
    const char *const COPPER_BULBS = "minecraft:trial_chambers_copper_bulb_degradation";

    // null location: the empty element; copper bulbs: the piece runs the bulb degradation
    struct ElementData {
        const char *mLocation;
        int32_t mWeight;
        bool mCopperBulbs;
    };

    struct PoolData {
        const char *mName;
        const char *mFallback;
        std::vector<ElementData> mElements;
    };

    // in the game's registration order; EMPTY_pot is the game's spelling: the pack holds empty_pot, so the game
    // finds no template for it and tries the next element
    const std::vector<PoolData> POOLS = {
            {"minecraft:trial_chambers/chamber/end", "minecraft:empty", {
                    {"minecraft:trial_chambers/corridor/end_1", 1, true},
                    {"minecraft:trial_chambers/corridor/end_2", 1, true}}},
            {"minecraft:trial_chambers/chamber/entrance_cap", "minecraft:trial_chambers/chamber/entrance_cap", {
                    {"minecraft:trial_chambers/chamber/entrance_cap", 1, true}}},
            {"minecraft:trial_chambers/chambers/end", "minecraft:trial_chambers/hallway/fallback", {
                    {"minecraft:trial_chambers/chamber/chamber_1", 1, true},
                    {"minecraft:trial_chambers/chamber/assembly", 1, true},
                    {"minecraft:trial_chambers/chamber/eruption", 1, true},
                    {"minecraft:trial_chambers/chamber/slanted", 1, true}}},
            {"minecraft:trial_chambers/corridor", "minecraft:empty", {
                    {"minecraft:trial_chambers/corridor/second_plate", 1, false},
                    {"minecraft:trial_chambers/intersection/intersection_1", 1, true},
                    {"minecraft:trial_chambers/intersection/intersection_2", 1, true},
                    {"minecraft:trial_chambers/intersection/intersection_3", 1, true},
                    {"minecraft:trial_chambers/corridor/first_plate", 1, false},
                    {"minecraft:trial_chambers/corridor/atrium_1", 1, true},
                    {"minecraft:trial_chambers/corridor/entrance_1", 1, true},
                    {"minecraft:trial_chambers/corridor/entrance_2", 1, true},
                    {"minecraft:trial_chambers/corridor/entrance_3", 1, true}}},
            {"minecraft:trial_chambers/chamber/addon", "minecraft:empty", {
                    {"minecraft:trial_chambers/chamber/addon/full_stacked_walkway", 1, false},
                    {"minecraft:trial_chambers/chamber/addon/full_stacked_walkway_2", 1, false},
                    {"minecraft:trial_chambers/chamber/addon/full_corner_column", 1, false},
                    {"minecraft:trial_chambers/chamber/addon/grate_bridge", 1, false},
                    {"minecraft:trial_chambers/chamber/addon/hanging_platform", 1, false},
                    {"minecraft:trial_chambers/chamber/addon/short_grate_platform", 1, false},
                    {"minecraft:trial_chambers/chamber/addon/short_platform", 1, false},
                    {"minecraft:trial_chambers/chamber/addon/lower_staircase_down", 1, false},
                    {"minecraft:trial_chambers/chamber/addon/walkway_with_bridge_1", 1, false},
                    {"minecraft:trial_chambers/chamber/addon/c1_breeze", 1, false}}},
            {"minecraft:trial_chambers/chamber/assembly", "minecraft:empty", {
                    {"minecraft:trial_chambers/chamber/assembly/full_column", 2, false},
                    {"minecraft:trial_chambers/chamber/assembly/cover_1", 2, false},
                    {"minecraft:trial_chambers/chamber/assembly/cover_2", 2, false},
                    {"minecraft:trial_chambers/chamber/assembly/cover_3", 2, false},
                    {"minecraft:trial_chambers/chamber/assembly/cover_4", 2, false},
                    {"minecraft:trial_chambers/chamber/assembly/cover_5", 2, false},
                    {"minecraft:trial_chambers/chamber/assembly/cover_6", 2, false},
                    {"minecraft:trial_chambers/chamber/assembly/cover_7", 5, false},
                    {"minecraft:trial_chambers/chamber/assembly/platform_1", 2, false},
                    {"minecraft:trial_chambers/chamber/assembly/spawner_1", 1, false},
                    {"minecraft:trial_chambers/chamber/assembly/hanging_1", 2, false},
                    {"minecraft:trial_chambers/chamber/assembly/hanging_2", 1, false},
                    {"minecraft:trial_chambers/chamber/assembly/hanging_3", 2, false},
                    {"minecraft:trial_chambers/chamber/assembly/hanging_4", 2, false},
                    {"minecraft:trial_chambers/chamber/assembly/hanging_5", 4, false},
                    {"minecraft:trial_chambers/chamber/assembly/left_staircase_1", 1, false},
                    {"minecraft:trial_chambers/chamber/assembly/left_staircase_2", 1, false},
                    {"minecraft:trial_chambers/chamber/assembly/left_staircase_3", 1, false},
                    {"minecraft:trial_chambers/chamber/assembly/right_staircase_1", 1, false},
                    {"minecraft:trial_chambers/chamber/assembly/right_staircase_2", 1, false},
                    {"minecraft:trial_chambers/chamber/assembly/right_staircase_3", 1, false}}},
            {"minecraft:trial_chambers/chamber/eruption", "minecraft:empty", {
                    {"minecraft:trial_chambers/chamber/eruption/center_1", 1, false},
                    {"minecraft:trial_chambers/chamber/eruption/breeze_slice_1", 1, false},
                    {"minecraft:trial_chambers/chamber/eruption/slice_1", 1, false},
                    {"minecraft:trial_chambers/chamber/eruption/slice_2", 1, false},
                    {"minecraft:trial_chambers/chamber/eruption/slice_3", 1, false},
                    {"minecraft:trial_chambers/chamber/eruption/quadrant_1", 1, false},
                    {"minecraft:trial_chambers/chamber/eruption/quadrant_2", 1, false},
                    {"minecraft:trial_chambers/chamber/eruption/quadrant_3", 1, false},
                    {"minecraft:trial_chambers/chamber/eruption/quadrant_4", 1, false},
                    {"minecraft:trial_chambers/chamber/eruption/quadrant_5", 1, false}}},
            {"minecraft:trial_chambers/chamber/slanted", "minecraft:empty", {
                    {"minecraft:trial_chambers/chamber/slanted/center", 1, false},
                    {"minecraft:trial_chambers/chamber/slanted/hallway_1", 1, false},
                    {"minecraft:trial_chambers/chamber/slanted/hallway_2", 1, false},
                    {"minecraft:trial_chambers/chamber/slanted/hallway_3", 1, false},
                    {"minecraft:trial_chambers/chamber/slanted/hallway_4", 1, false},
                    {"minecraft:trial_chambers/chamber/slanted/hallway_5", 1, false},
                    {"minecraft:trial_chambers/chamber/slanted/quadrant_1", 1, false},
                    {"minecraft:trial_chambers/chamber/slanted/quadrant_2", 1, false},
                    {"minecraft:trial_chambers/chamber/slanted/quadrant_3", 1, false},
                    {"minecraft:trial_chambers/chamber/slanted/quadrant_4", 1, false},
                    {"minecraft:trial_chambers/chamber/slanted/ramp_1", 1, false},
                    {"minecraft:trial_chambers/chamber/slanted/ramp_2", 1, false},
                    {"minecraft:trial_chambers/chamber/slanted/ramp_3", 1, false},
                    {"minecraft:trial_chambers/chamber/slanted/ramp_4", 1, false},
                    {"minecraft:trial_chambers/chamber/slanted/ominous_upper_arm_1", 1, false}}},
            {"minecraft:trial_chambers/chamber/pedestal", "minecraft:empty", {
                    {"minecraft:trial_chambers/chamber/pedestal/center_1", 1, false},
                    {"minecraft:trial_chambers/chamber/pedestal/slice_1", 1, false},
                    {"minecraft:trial_chambers/chamber/pedestal/slice_2", 3, false},
                    {"minecraft:trial_chambers/chamber/pedestal/slice_3", 3, false},
                    {"minecraft:trial_chambers/chamber/pedestal/slice_4", 3, false},
                    {"minecraft:trial_chambers/chamber/pedestal/slice_5", 3, false},
                    {"minecraft:trial_chambers/chamber/pedestal/ominous_slice_1", 1, false},
                    {"minecraft:trial_chambers/chamber/pedestal/quadrant_1", 1, false},
                    {"minecraft:trial_chambers/chamber/pedestal/quadrant_2", 1, false},
                    {"minecraft:trial_chambers/chamber/pedestal/quadrant_3", 1, false},
                    {"minecraft:trial_chambers/chamber/slanted/quadrant_1", 1, false},
                    {"minecraft:trial_chambers/chamber/slanted/quadrant_2", 1, false},
                    {"minecraft:trial_chambers/chamber/slanted/quadrant_3", 1, false},
                    {"minecraft:trial_chambers/chamber/slanted/quadrant_4", 1, false}}},
            {"minecraft:trial_chambers/corridor/slices", "minecraft:empty", {
                    {"minecraft:trial_chambers/corridor/straight_1", 1, true},
                    {"minecraft:trial_chambers/corridor/straight_2", 2, true},
                    {"minecraft:trial_chambers/corridor/straight_3", 2, true},
                    {"minecraft:trial_chambers/corridor/straight_4", 2, true},
                    {"minecraft:trial_chambers/corridor/straight_5", 2, true},
                    {"minecraft:trial_chambers/corridor/straight_6", 2, true},
                    {"minecraft:trial_chambers/corridor/straight_7", 1, true},
                    {"minecraft:trial_chambers/corridor/straight_8", 2, true}}},
            {"minecraft:trial_chambers/hallway/fallback", "minecraft:empty", {
                    {"minecraft:trial_chambers/hallway/rubble", 1, false},
                    {"minecraft:trial_chambers/hallway/rubble_chamber", 1, false},
                    {"minecraft:trial_chambers/hallway/rubble_thin", 1, false},
                    {"minecraft:trial_chambers/hallway/rubble_chamber_thin", 1, false}}},
            {"minecraft:trial_chambers/hallway", "minecraft:trial_chambers/hallway/fallback", {
                    {"minecraft:trial_chambers/hallway/corridor_connector_1", 1, false},
                    {"minecraft:trial_chambers/hallway/upper_hallway_connector", 1, true},
                    {"minecraft:trial_chambers/hallway/lower_hallway_connector", 1, true},
                    {"minecraft:trial_chambers/hallway/rubble", 1, false},
                    {"minecraft:trial_chambers/chamber/chamber_1", 150, true},
                    {"minecraft:trial_chambers/chamber/chamber_2", 150, true},
                    {"minecraft:trial_chambers/chamber/chamber_4", 150, true},
                    {"minecraft:trial_chambers/chamber/chamber_8", 150, true},
                    {"minecraft:trial_chambers/chamber/assembly", 150, true},
                    {"minecraft:trial_chambers/chamber/eruption", 150, true},
                    {"minecraft:trial_chambers/chamber/slanted", 150, true},
                    {"minecraft:trial_chambers/chamber/pedestal", 150, true},
                    {"minecraft:trial_chambers/hallway/rubble_chamber", 10, true},
                    {"minecraft:trial_chambers/hallway/rubble_chamber_thin", 1, true},
                    {"minecraft:trial_chambers/hallway/cache_1", 1, true},
                    {"minecraft:trial_chambers/hallway/left_corner", 1, true},
                    {"minecraft:trial_chambers/hallway/right_corner", 1, true},
                    {"minecraft:trial_chambers/hallway/corner_staircase", 1, true},
                    {"minecraft:trial_chambers/hallway/corner_staircase_down", 1, true},
                    {"minecraft:trial_chambers/hallway/long_straight_staircase", 1, true},
                    {"minecraft:trial_chambers/hallway/long_straight_staircase_down", 1, true},
                    {"minecraft:trial_chambers/hallway/straight", 1, true},
                    {"minecraft:trial_chambers/hallway/straight_staircase", 1, true},
                    {"minecraft:trial_chambers/hallway/straight_staircase_down", 1, true}}},
            {"minecraft:trial_chambers/corridors/addon/lower", "minecraft:empty", {
                    {nullptr, 8, false}, {"minecraft:trial_chambers/corridor/addon/staircase", 1, false},
                    {"minecraft:trial_chambers/corridor/addon/wall", 1, false},
                    {"minecraft:trial_chambers/corridor/addon/ladder_to_middle", 1, false},
                    {"minecraft:trial_chambers/corridor/addon/arrow_dispenser", 1, false},
                    {"minecraft:trial_chambers/corridor/addon/bridge_lower", 2, false}}},
            {"minecraft:trial_chambers/corridors/addon/middle", "minecraft:empty", {
                    {nullptr, 8, false}, {"minecraft:trial_chambers/corridor/addon/open_walkway", 2, false},
                    {"minecraft:trial_chambers/corridor/addon/walled_walkway", 1, false}}},
            {"minecraft:trial_chambers/corridors/addon/middle_upper", "minecraft:empty", {
                    {nullptr, 6, false}, {"minecraft:trial_chambers/corridor/addon/open_walkway_upper", 2, false},
                    {"minecraft:trial_chambers/corridor/addon/chandelier_upper", 1, false},
                    {"minecraft:trial_chambers/corridor/addon/decoration_upper", 1, false},
                    {"minecraft:trial_chambers/corridor/addon/head_upper", 1, false},
                    {"minecraft:trial_chambers/corridor/addon/reward_upper", 1, false}}},
            {"minecraft:trial_chambers/atrium", "minecraft:empty", {
                    {"minecraft:trial_chambers/corridor/atrium/bogged_relief", 1, false},
                    {"minecraft:trial_chambers/corridor/atrium/breeze_relief", 1, false},
                    {"minecraft:trial_chambers/corridor/atrium/spiral_relief", 1, false},
                    {"minecraft:trial_chambers/corridor/atrium/spider_relief", 1, false},
                    {"minecraft:trial_chambers/corridor/atrium/grand_staircase_1", 1, false},
                    {"minecraft:trial_chambers/corridor/atrium/grand_staircase_2", 1, false},
                    {"minecraft:trial_chambers/corridor/atrium/grand_staircase_3", 1, false}}},
            {"minecraft:trial_chambers/decor", "minecraft:empty", {
                    {nullptr, 22, false}, {"minecraft:trial_chambers/decor/EMPTY_pot", 2, false},
                    {"minecraft:trial_chambers/decor/dead_bush_pot", 2, false},
                    {"minecraft:trial_chambers/decor/undecorated_pot", 10, false},
                    {"minecraft:trial_chambers/decor/flow_pot", 1, false},
                    {"minecraft:trial_chambers/decor/guster_pot", 1, false},
                    {"minecraft:trial_chambers/decor/scrape_pot", 1, false},
                    {"minecraft:trial_chambers/decor/candle_1", 1, false},
                    {"minecraft:trial_chambers/decor/candle_2", 1, false},
                    {"minecraft:trial_chambers/decor/candle_3", 1, false},
                    {"minecraft:trial_chambers/decor/candle_4", 1, false},
                    {"minecraft:trial_chambers/decor/barrel", 2, false}}},
            {"minecraft:trial_chambers/entrance", "minecraft:empty", {
                    {"minecraft:trial_chambers/corridor/addon/display_1", 1, false},
                    {"minecraft:trial_chambers/corridor/addon/display_2", 1, false},
                    {"minecraft:trial_chambers/corridor/addon/display_3", 1, false}}},
            {"minecraft:trial_chambers/decor/chamber", "minecraft:empty", {
                    {nullptr, 4, false}, {"minecraft:trial_chambers/decor/undecorated_pot", 1, false}}},
            {"minecraft:trial_chambers/reward/all", "minecraft:empty", {
                    {"minecraft:trial_chambers/reward/vault", 1, false}}},
            {"minecraft:trial_chambers/reward/ominous_vault", "minecraft:empty", {
                    {"minecraft:trial_chambers/reward/ominous_vault", 1, false}}},
            {"minecraft:trial_chambers/reward/contents/default", "minecraft:empty", {
                    {"minecraft:trial_chambers/reward/vault", 1, false}}},
            {"minecraft:trial_chambers/chests/supply", "minecraft:empty", {
                    {"minecraft:trial_chambers/chests/connectors/supply", 1, false}}},
            {"minecraft:trial_chambers/chests/contents/supply", "minecraft:empty", {
                    {"minecraft:trial_chambers/chests/supply", 1, false}}},
            {"minecraft:trial_chambers/spawner/ranged", "minecraft:empty", {
                    {"minecraft:trial_chambers/spawner/connectors/ranged", 1, false}}},
            {"minecraft:trial_chambers/spawner/slow_ranged", "minecraft:empty", {
                    {"minecraft:trial_chambers/spawner/connectors/slow_ranged", 1, false}}},
            {"minecraft:trial_chambers/spawner/melee", "minecraft:empty", {
                    {"minecraft:trial_chambers/spawner/connectors/melee", 1, false}}},
            {"minecraft:trial_chambers/spawner/small_melee", "minecraft:empty", {
                    {"minecraft:trial_chambers/spawner/connectors/small_melee", 1, false}}},
            {"minecraft:trial_chambers/spawner/breeze", "minecraft:empty", {
                    {"minecraft:trial_chambers/spawner/connectors/breeze", 1, false}}},
            {"minecraft:trial_chambers/spawner/all", "minecraft:empty", {
                    {"minecraft:trial_chambers/spawner/connectors/ranged", 1, false},
                    {"minecraft:trial_chambers/spawner/connectors/melee", 1, false},
                    {"minecraft:trial_chambers/spawner/connectors/small_melee", 1, false}}},
            {"minecraft:trial_chambers/spawner/contents/breeze", "minecraft:empty", {
                    {"minecraft:trial_chambers/spawner/breeze/breeze", 1, false}}},
            {"minecraft:trial_chambers/dispensers/chamber", "minecraft:empty", {
                    {nullptr, 1, false}, {"minecraft:trial_chambers/dispensers/chamber", 1, false},
                    {"minecraft:trial_chambers/dispensers/wall_dispenser", 1, false},
                    {"minecraft:trial_chambers/dispensers/floor_dispenser", 1, false}}},
    };

    // the spawner pools the aliases point at: one template each, named like the pool
    const char *const SPAWNERS[] = {
            "minecraft:trial_chambers/spawner/ranged/skeleton",
            "minecraft:trial_chambers/spawner/slow_ranged/skeleton",
            "minecraft:trial_chambers/spawner/ranged/stray",
            "minecraft:trial_chambers/spawner/slow_ranged/stray",
            "minecraft:trial_chambers/spawner/ranged/poison_skeleton",
            "minecraft:trial_chambers/spawner/slow_ranged/poison_skeleton",
            "minecraft:trial_chambers/spawner/melee/zombie",
            "minecraft:trial_chambers/spawner/melee/husk",
            "minecraft:trial_chambers/spawner/melee/spider",
            "minecraft:trial_chambers/spawner/small_melee/slime",
            "minecraft:trial_chambers/spawner/small_melee/cave_spider",
            "minecraft:trial_chambers/spawner/small_melee/silverfish",
            "minecraft:trial_chambers/spawner/small_melee/baby_zombie",
    };

    // the game's list for the chambers: no deep dark, sulfur caves, dappled forest or ice caves
    const std::vector<int32_t> BIOMES = {
            48,  49,  16,  27,  28,  156, 155, 192, 26,  44,  30,  31,  158, 45,  47,  43,  24,  41,  2,   17, 130,
            188, 3,   20,  131, 34,  162, 132, 4,   18,  46,  183, 11,  185, 13,  12,  140, 182, 21,  23,  151, 22,
            149, 149, 10,  42,  187, 191, 186, 32,  33,  37,  165, 39,  167, 38,  166, 14,  15,  0,   193, 1,   161,
            160, 7,   29,  157, 35,  163, 36,  164, 184, 25,  189, 129, 6,   134, 5,   19,  133, 40};

    // one choice per mob, each pointing the aliases at that mob's pools
    JigsawStructures::AliasBinding bind(const std::vector<const char *> &aliases,
                                        const std::vector<const char *> &mobs) {
        JigsawStructures::AliasBinding binding;
        for (const char *mob: mobs) {
            JigsawStructures::AliasBinding::Choice choice;
            for (const char *alias: aliases) {
                const std::string name = alias;
                const std::string kind = name.substr(name.rfind('/') + 1);
                choice.mTargets.emplace_back(name, "minecraft:trial_chambers/spawner/" + kind + "/" + mob);
            }
            binding.mChoices.push_back(std::move(choice));
        }
        return binding;
    }
}

void JigsawStructures::_loadTrialChambers() {
    ProcessorList &bulbs = mProcessors[COPPER_BULBS];
    bulbs.mRules = {
            {StructureProcessors::Test::RandomBlockMatch, "minecraft:waxed_copper_bulb", 0.1f, nullptr,
             "minecraft:waxed_oxidized_copper_bulb", "lit", 1},
            {StructureProcessors::Test::RandomBlockMatch, "minecraft:waxed_copper_bulb", 0.33333334f, nullptr,
             "minecraft:waxed_weathered_copper_bulb", "lit", 1},
            {StructureProcessors::Test::RandomBlockMatch, "minecraft:waxed_copper_bulb", 0.5f, nullptr,
             "minecraft:waxed_exposed_copper_bulb", "lit", 1},
    };

    const auto element = [this](const char *location, const char *processors) {
        auto made = std::make_unique<JigsawPlacer::Element>();
        made->mLocation = location;
        made->mProcessors = processors;
        mElements.push_back(std::move(made));
        return mElements.back().get();
    };

    for (const PoolData &data: POOLS) {
        JigsawPlacer::Pool &pool = mPools[data.mName];
        pool.mFallback = data.mFallback;
        for (const ElementData &weighted: data.mElements) {
            const char *processors = weighted.mCopperBulbs ? COPPER_BULBS : "";
            const JigsawPlacer::Element *kept =
                    weighted.mLocation == nullptr ? nullptr : element(weighted.mLocation, processors);
            pool.mSlots.insert(pool.mSlots.end(), (size_t) weighted.mWeight, kept);
        }
    }
    for (const char *spawner: SPAWNERS) {
        JigsawPlacer::Pool &pool = mPools[spawner];
        pool.mFallback = "minecraft:empty";
        pool.mSlots.push_back(element(spawner, ""));
    }

    Structure structure;
    structure.mIdentifier = "minecraft:trial_chambers";
    structure.mBiomes = BIOMES;
    structure.mStartPool = "minecraft:trial_chambers/chamber/end";
    structure.mMaxDepth = 20;
    structure.mMaxDistanceHorizontal = 116;
    structure.mMaxDistanceVertical = 116;
    structure.mStartY = -40;
    structure.mStartYMax = -20;
    structure.mPaddingTop = 10;
    structure.mPaddingBottom = 10;
    structure.mAdaptation = TerrainBeard::Adaptation::Encapsulate;
    structure.mAdapts = true;

    // ranged and slow ranged spawners share one mob; melee and small melee draw their own
    structure.mAliases = {
            bind({"minecraft:trial_chambers/spawner/contents/ranged",
                  "minecraft:trial_chambers/spawner/contents/slow_ranged"},
                 {"skeleton", "stray", "poison_skeleton"}),
            bind({"minecraft:trial_chambers/spawner/contents/melee"}, {"zombie", "husk", "spider"}),
            bind({"minecraft:trial_chambers/spawner/contents/small_melee"},
                 {"slime", "cave_spider", "silverfish", "baby_zombie"})};
    mStructures[structure.mIdentifier] = std::move(structure);

    auto set = std::make_unique<Set>();
    set->mIdentifier = "minecraft:trial_chambers";
    set->mSalt = 94251327;
    set->mSpacing = 34;
    set->mSeparation = 12;
    set->mStructures.push_back({this->structure("minecraft:trial_chambers"), 1});
    _addSet(std::move(set));
}
