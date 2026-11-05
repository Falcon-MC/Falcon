#include "World/Level/LevelGen/Overworld/Feature/OverworldRulePlacer.h"

#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/FenceBlock.h"
#include "World/Level/Block/FloweredAzaleaLeavesBlock.h"
#include "World/Level/Block/LeavesBlock.h"
#include "World/Level/Block/MagmaBlock.h"
#include "World/Level/Block/SoulSandBlock.h"
#include "World/Level/Block/Systems/LiquidPhysicsSystem.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/WaterBlock.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/BlockStateSet.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Feature/FeatureExpression.h"
#include "World/Level/LevelGen/Feature/IFeature.h"
#include "World/Level/LevelGen/Overworld/Biome/BiomeTemperature.h"
#include "World/Level/LevelGen/Overworld/Surface/SurfaceSimplexNoise.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {
    constexpr float PI = 3.1415927f;
    constexpr const char *UNDERGROUND_PASS = "underground_pass";
    constexpr const char *SURFACE_PASS = "surface_pass";
    constexpr const char *AFTER_SURFACE_PASS = "after_surface_pass";

    // the passes the game places after the carvers, in order (sub_1471CE3F0)
    const char *const PASSES[] = {
            "first_pass", "before_underground_pass", UNDERGROUND_PASS, "after_underground_pass",
            "before_surface_pass", SURFACE_PASS, "after_surface_pass", "before_sky_pass", "sky_pass",
            "after_sky_pass", "final_pass"
    };

    struct ReplaceRule {
        Block (*mPlaces)();
        std::vector<std::string> mMayReplace;
    };

    // Random::nextInt(n) (sub_141A7E190)
    int32_t nextInt(std::mt19937 &random, int32_t bound) {
        return bound != 0 ? static_cast<int32_t>(random() % static_cast<uint32_t>(bound)) : 0;
    }

    // Random::nextFloat (sub_141A7E230)
    float nextFloat(std::mt19937 &random) {
        return static_cast<float>(static_cast<double>(random()) * 2.3283064365386963e-10);
    }

    // the game's sine table, filled with sinf (sub_14199D950)
    const std::vector<float> &sineTable() {
        static const std::vector<float> table = [] {
            std::vector<float> values(65536);
            for (int32_t i = 0; i < 65536; i++)
                values[(size_t) i] = std::sin(static_cast<float>(i) / 10430.378f);
            return values;
        }();
        return table;
    }

    float sineAt(float scaled) {
        return sineTable()[static_cast<uint16_t>(static_cast<int32_t>(scaled))];
    }

    // HashedString's hash (FNV-1, 64 bits), kept by each feature rule
    uint64_t hashedString(const std::string &value) {
        uint64_t hash = 0xCBF29CE484222325ULL;
        for (char character: value)
            hash = (hash * 0x100000001B3ULL) ^ static_cast<uint8_t>(character);
        return hash;
    }

    using ExpressionPtr = std::shared_ptr<const FeatureExpression>;

    // one axis of a scatter: a single value or a distribution between two bounds
    struct Axis {
        int32_t mDistribution = 0;
        ExpressionPtr mMin;
        ExpressionPtr mMax;
        bool mReadsWorld = false;
    };

    bool readsWorld(const std::string &text) {
        return text.find("worldx") != std::string::npos || text.find("worldz") != std::string::npos;
    }

    // uniform and triangle are the only distributions the ported rules use
    bool parseAxis(int32_t distribution, const std::string &min, const std::string &max, Axis &axis) {
        axis.mDistribution = distribution;
        axis.mMin = FeatureExpression::parse(min);
        axis.mReadsWorld = readsWorld(min);
        if (axis.mMin == nullptr)
            return false;
        if (distribution == 0)
            return true;

        axis.mMax = FeatureExpression::parse(max);
        axis.mReadsWorld = axis.mReadsWorld || readsWorld(max);
        return axis.mMax != nullptr && (distribution == 1 || distribution == 6);
    }

    // sub_1437FD570
    int32_t distribute(int32_t distribution, int32_t min, int32_t max, std::mt19937 &random) {
        if (distribution == 1)
            return nextInt(random, max - min) + min;

        const int32_t range = max - min;
        const int32_t rest = range - range / 2;
        const int32_t first = range >= -1 ? nextInt(random, range / 2 + 1) : 0;
        const int32_t second = rest >= 0 ? nextInt(random, rest + 1) : 0;
        return min + second + first;
    }

    // query.noise: one octave of simplex noise from the game's Mersenne twister seeded with 2345 (sub_145E2C470)
    float queryNoise(float x, float z) {
        static const SurfaceSimplexNoise noise = [] {
            std::mt19937 random(2345);
            return SurfaceSimplexNoise(random);
        }();
        return noise.getValue(x, z);
    }

    // query.heightmap (sub_145E2C6D0) reads the chunk's heightmap, which the game computes once the terrain is
    // carved and does not update while features are placed; the generator keeps the highest non-air block there
    int32_t heightmapAt(Level &level, int32_t x, int32_t z) {
        LevelChunk *chunk = level.peekChunkPtr(x >> 4, z >> 4);
        if (chunk == nullptr)
            return LevelChunk::MIN_Y;

        return chunk->getHeight(x & 15, z & 15) + 1;
    }
    // axis visiting order of each coordinate_eval_order (sub_1437FD990)
    const int32_t EVAL_ORDERS[6][3] = {
            {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}
    };

    struct Sphere {
        float mX;
        float mY;
        float mZ;
        float mRadiusSquared;
        float mRadius;
    };

    bool contains(const std::vector<std::string> &names, const BlockState &state) {
        for (const std::string &name: names) {
            if (state.mName == name)
                return true;
        }
        return false;
    }

    // sub_14A2C9860
    bool exposedToAir(BlockManager &blocks, int32_t x, int32_t y, int32_t z) {
        return blocks.getBlockAt(x, y - 1, z).mName == "minecraft:air"
               || blocks.getBlockAt(x, y + 1, z).mName == "minecraft:air"
               || blocks.getBlockAt(x, y, z - 1).mName == "minecraft:air"
               || blocks.getBlockAt(x, y, z + 1).mName == "minecraft:air"
               || blocks.getBlockAt(x - 1, y, z).mName == "minecraft:air"
               || blocks.getBlockAt(x + 1, y, z).mName == "minecraft:air";
    }

    // LevelChunk::getAboveTopSolidBlock (sub_1438650A0) as the query calls it, water and leaves not counted: leaves
    // (material type 7) are looked through, and the first solid block below them ends the scan
    int32_t aboveTopSolid(BlockManager &blocks, int32_t x, int32_t z) {
        for (int32_t y = LevelChunk::MAX_Y; y >= LevelChunk::MIN_Y; y--) {
            const BlockState &state = blocks.getBlockAt(x, y, z);
            if (state.mName == "minecraft:air")
                continue;
            if (VanillaBlocks::getAs<LeavesBlock>(state.mName) != nullptr
                || VanillaBlocks::getAs<FloweredAzaleaLeavesBlock>(state.mName) != nullptr)
                continue;

            if (BlockSupport::isSolid(state))
                return y + 1;
        }
        return LevelChunk::MIN_Y;
    }

    // the Molang variables of a scatter: its origin, then each axis once drawn (sub_1437FBE80, sub_1437FD990)
    class ScatterVariables : public FeatureExpression::Context {
    public:
        ScatterVariables(BlockManager &blocks, const int32_t origin[3]) : mBlocks(blocks) {
            for (size_t axis = 0; axis < 3; axis++) {
                mOrigin[axis] = static_cast<float>(origin[axis]);
                mWorld[axis] = 0.0f;
            }
        }

        float variable(const std::string &name) const override {
            static const char *const ORIGINS[] = {"originx", "originy", "originz"};
            static const char *const WORLDS[] = {"worldx", "worldy", "worldz"};
            for (size_t axis = 0; axis < 3; axis++) {
                if (name == ORIGINS[axis])
                    return mOrigin[axis];
                if (name == WORLDS[axis])
                    return mWorld[axis];
            }
            // is_legacy: worlds generated before the 1.18 terrain; any other name reads as 0 too
            return 0.0f;
        }

        float query(const std::string &name, float first, float second) const override {
            const auto x = static_cast<int32_t>(std::round(first));
            const auto z = static_cast<int32_t>(std::round(second));
            if (name == "heightmap")
                return static_cast<float>(heightmapAt(mBlocks.getLevel(), x, z));
            if (name == "above_top_solid")
                return static_cast<float>(aboveTopSolid(mBlocks, x, z));
            if (name == "noise")
                return queryNoise(first, second);
            return 0.0f;
        }

        void setWorld(size_t axis, int32_t value) {
            mWorld[axis] = static_cast<float>(value);
        }

    private:
        BlockManager &mBlocks;
        float mOrigin[3];
        float mWorld[3];
    };

    // sub_143815010
    int32_t sampleAxis(const Axis &axis, const ScatterVariables &variables, std::mt19937 &random) {
        const auto min = static_cast<int32_t>(std::round(axis.mMin->evaluate(variables)));
        if (axis.mDistribution == 0)
            return min;

        const auto max = static_cast<int32_t>(std::round(axis.mMax->evaluate(variables)));
        return max > min ? distribute(axis.mDistribution, min, max, random) : min;
    }

    // the scatter's chance, then its iteration count (sub_1437FC150)
    int32_t iterationsOf(int32_t numerator, int32_t denominator, const ExpressionPtr &percent,
                         const ExpressionPtr &iterations, const ScatterVariables &variables, std::mt19937 &random) {
        if (numerator > 0 && denominator > 0) {
            if (numerator != denominator && nextInt(random, denominator) >= numerator)
                return 0;
        } else {
            const float chance = percent->evaluate(variables);
            if (chance < 100.0f && (chance <= 0.0f || chance <= nextFloat(random) * 100.0f))
                return 0;
        }

        const float rounded = std::round(iterations->evaluate(variables));
        return rounded > 0.0f ? static_cast<int32_t>(rounded) : 0;
    }

    // each axis in the scatter's order, from the origin (sub_1437FD990)
    void scatterPosition(const Axis axes[3], int32_t evalOrder, const int32_t origin[3], ScatterVariables &variables,
                         std::mt19937 &random, int32_t position[3]) {
        for (int32_t axis: EVAL_ORDERS[evalOrder]) {
            position[axis] = origin[axis] + sampleAxis(axes[axis], variables, random);
            variables.setWorld((size_t) axis, position[axis]);
        }
    }

    // a query reading worldx and worldz must come after both are drawn
    bool drawnInOrder(const Axis axes[3], int32_t evalOrder) {
        bool drawnX = false;
        bool drawnZ = false;
        for (int32_t axis: EVAL_ORDERS[evalOrder]) {
            if (axes[axis].mReadsWorld && !(drawnX && drawnZ))
                return false;
            drawnX = drawnX || axis == 0;
            drawnZ = drawnZ || axis == 2;
        }
        return true;
    }

    bool isWater(const BlockState &state) {
        return VanillaBlocks::getAs<WaterBlock>(state.mName) != nullptr;
    }

    // the solid blocking test of the feature placers (BlockSource vfunc 432, sub_1432D1C00)
    bool isSolidBlocking(BlockManager &blocks, int32_t x, int32_t y, int32_t z) {
        return BlockSupport::isSolid(blocks.getBlockAt(x, y, z));
    }

    // kelp and seagrass may not stand on magma, soul sand or a fence (the list at xmmword_1529B0DB0)
    bool refusesWaterPlant(const BlockState &below) {
        static const BlockStateSet REFUSED = BlockStateSet::ofBlocks([](const Block &block) {
            return dynamic_cast<const MagmaBlock *>(&block) != nullptr
                   || dynamic_cast<const SoulSandBlock *>(&block) != nullptr
                   || dynamic_cast<const FenceBlock *>(&block) != nullptr;
        });
        return REFUSED.contains(below);
    }

    // a block placed in water keeps the water as its second block when it can hold it
    void setInWater(BlockManager &blocks, int32_t x, int32_t y, int32_t z, const BlockState &state) {
        const BlockState replaced = blocks.getBlockAt(x, y, z);
        blocks.setBlockStateAt(x, y, z, state);
        if (isWater(replaced) && LiquidPhysicsSystem::getWaterloggingLevel(state) > 0)
            blocks.setExtraBlockAt(x, y, z, replaced);
    }

    BlockState withState(const BlockState &state, const char *name, int32_t value) {
        Tag states = state.mStates;
        states.putInt(name, value);
        return BlockState(state.mName, states);
    }

    BlockState withState(const BlockState &state, const char *name, const char *value) {
        Tag states = state.mStates;
        states.putString(name, value);
        return BlockState(state.mName, states);
    }

    // the two (sub_140391CC0 & 7) differences of the kelp and seagrass jitter, z drawn first
    void jitter(std::mt19937 &random, int32_t &dx, int32_t &dz) {
        const uint32_t firstZ = random() & 7u;
        dz = static_cast<int32_t>(firstZ) - static_cast<int32_t>(random() & 7u);
        const uint32_t firstX = random() & 7u;
        dx = static_cast<int32_t>(firstX) - static_cast<int32_t>(random() & 7u);
    }
}

// a minecraft:ore_feature of the behavior packs
struct OverworldRulePlacer::OreDefinition {
    const char *mIdentifier;
    int32_t mCount;
    float mDiscardChanceOnAirExposure;
    std::vector<ReplaceRule> mRules;
};

// one of the game's own disk features, registered in sub_14726F340
struct OverworldRulePlacer::DiskDefinition {
    const char *mIdentifier;
    Block (*mPlaces)();
    int32_t mRadius;
    int32_t mHalfHeight;
    // dirt, coarse dirt and one block of the disk's own
    BlockStateSet mReplaces;
    // the clay disk also starts on clay, not only in water
    bool mStartsOnItself;
};

// the game's own seagrass features (SeagrassFeature, vtable 0x14F2F24E0, registered in sub_14726F340 by
// sub_14728CFA0); the count is 1 for worlds of 1.21.60 and later
struct OverworldRulePlacer::SeagrassDefinition {
    const char *mIdentifier;
    int32_t mCount;
    // the chance of a single plant, tall seagrass otherwise
    float mShortChance;
};

// a minecraft:scatter_feature: each axis a single value or a distribution around the position it is given
struct OverworldRulePlacer::ScatterDefinition {
    struct AxisText {
        int32_t mDistribution;
        const char *mMin;
        const char *mMax;
    };

    const char *mIdentifier;
    const char *mIterations;
    AxisText mAxes[3];
    int32_t mEvalOrder;
    Placed mPlaces;
    // scatter_chance as a fraction, every time when 0
    int32_t mChanceNumerator = 0;
    int32_t mChanceDenominator = 0;
};

// a minecraft:single_block_feature with one block, its survival enforced, replacing only air
struct OverworldRulePlacer::SingleBlockDefinition {
    const char *mIdentifier;
    Block (*mBlock)();
};

// a minecraft:weighted_random_feature
struct OverworldRulePlacer::WeightedDefinition {
    const char *mIdentifier;
    std::vector<std::pair<Placed, int32_t>> mFeatures;
};

OverworldRulePlacer::Feature OverworldRulePlacer::_find(const BiomeFeatureEntry &entry) {
    // BDS 1.26.61 behavior packs, features/*.json
    static const std::vector<OreDefinition> ORES = {
            {"minecraft:andesite_feature", 64, 0.0f, {{&VanillaBlocks::ANDESITE, {"minecraft:stone"}}}},
            {"minecraft:clay_ore_feature", 33, 0.0f, {{&VanillaBlocks::CLAY, {"minecraft:stone"}}}},
            {"minecraft:coal_ore_feature", 17, 0.5f, {{&VanillaBlocks::COAL_ORE, {"minecraft:stone"}},
                                                      {&VanillaBlocks::DEEPSLATE_COAL_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:coal_ore_upper_feature", 17, 0.0f, {{&VanillaBlocks::COAL_ORE, {"minecraft:stone"}},
                                                            {&VanillaBlocks::DEEPSLATE_COAL_ORE,
                                                             {"minecraft:deepslate"}}}},
            {"minecraft:copper_ore_feature", 10, 0.0f, {{&VanillaBlocks::COPPER_ORE, {"minecraft:stone"}},
                                                        {&VanillaBlocks::DEEPSLATE_COPPER_ORE,
                                                         {"minecraft:deepslate"}}}},
            {"minecraft:diamond_ore_buried_feature", 8, 1.0f, {{&VanillaBlocks::DIAMOND_ORE, {"minecraft:stone"}},
                                                               {&VanillaBlocks::DEEPSLATE_DIAMOND_ORE,
                                                                {"minecraft:deepslate"}}}},
            {"minecraft:diamond_ore_feature", 4, 0.5f, {{&VanillaBlocks::DIAMOND_ORE, {"minecraft:stone"}},
                                                        {&VanillaBlocks::DEEPSLATE_DIAMOND_ORE,
                                                         {"minecraft:deepslate"}}}},
            {"minecraft:diamond_ore_large_feature", 12, 0.7f, {{&VanillaBlocks::DIAMOND_ORE, {"minecraft:stone"}},
                                                               {&VanillaBlocks::DEEPSLATE_DIAMOND_ORE,
                                                                {"minecraft:deepslate"}}}},
            // not in the packs, only in the game's definitions/features/dirt_feature.json
            {"minecraft:dirt_feature", 33, 0.0f, {{&VanillaBlocks::DIRT, {"minecraft:stone"}}}},
            {"minecraft:diorite_feature", 64, 0.0f, {{&VanillaBlocks::DIORITE, {"minecraft:stone"}}}},
            {"minecraft:dripstone_caves_copper_ore_feature", 20, 0.0f,
             {{&VanillaBlocks::COPPER_ORE, {"minecraft:stone"}},
              {&VanillaBlocks::DEEPSLATE_COPPER_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:emerald_ore_feature", 3, 0.0f, {{&VanillaBlocks::EMERALD_ORE, {"minecraft:stone"}},
                                                        {&VanillaBlocks::DEEPSLATE_EMERALD_ORE,
                                                         {"minecraft:deepslate"}}}},
            {"minecraft:gold_ore_extra_feature", 9, 0.0f, {{&VanillaBlocks::GOLD_ORE, {"minecraft:stone"}},
                                                           {&VanillaBlocks::DEEPSLATE_GOLD_ORE,
                                                            {"minecraft:deepslate"}}}},
            {"minecraft:gold_ore_feature", 9, 0.5f, {{&VanillaBlocks::GOLD_ORE, {"minecraft:stone"}},
                                                     {&VanillaBlocks::DEEPSLATE_GOLD_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:gold_ore_lower_feature", 9, 0.5f, {{&VanillaBlocks::GOLD_ORE, {"minecraft:stone"}},
                                                           {&VanillaBlocks::DEEPSLATE_GOLD_ORE,
                                                            {"minecraft:deepslate"}}}},
            {"minecraft:granite_feature", 64, 0.0f, {{&VanillaBlocks::GRANITE, {"minecraft:stone"}}}},
            {"minecraft:gravel_ore_feature", 33, 0.0f, {{&VanillaBlocks::GRAVEL,
                                                         {"minecraft:stone", "minecraft:deepslate"}}}},
            {"minecraft:iron_ore_feature", 10, 0.0f, {{&VanillaBlocks::IRON_ORE, {"minecraft:stone"}},
                                                      {&VanillaBlocks::DEEPSLATE_IRON_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:iron_ore_small_feature", 4, 0.0f, {{&VanillaBlocks::IRON_ORE, {"minecraft:stone"}},
                                                           {&VanillaBlocks::DEEPSLATE_IRON_ORE,
                                                            {"minecraft:deepslate"}}}},
            {"minecraft:lapis_ore_buried_feature", 7, 1.0f, {{&VanillaBlocks::LAPIS_ORE, {"minecraft:stone"}},
                                                             {&VanillaBlocks::DEEPSLATE_LAPIS_ORE,
                                                              {"minecraft:deepslate"}}}},
            {"minecraft:lapis_ore_feature", 7, 0.0f, {{&VanillaBlocks::LAPIS_ORE, {"minecraft:stone"}},
                                                      {&VanillaBlocks::DEEPSLATE_LAPIS_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:redstone_ore_feature", 8, 0.0f, {{&VanillaBlocks::REDSTONE_ORE, {"minecraft:stone"}},
                                                         {&VanillaBlocks::DEEPSLATE_REDSTONE_ORE,
                                                          {"minecraft:deepslate"}}}},
            {"minecraft:tuff_feature", 64, 0.0f, {{&VanillaBlocks::TUFF, {"minecraft:stone", "minecraft:deepslate"}}}}
    };

    // vtables 0x14F4A0DC0 (clay, place sub_14D1FBFA0) and 0x14F4A1710 (sand and gravel, place sub_14D22F1B0)
    static const DiskDefinition DISKS[] = {
            {"minecraft:sand_feature", &VanillaBlocks::SAND, 7, 2,
             {"minecraft:dirt", "minecraft:coarse_dirt", "minecraft:grass_block"}, false},
            {"minecraft:gravel_feature", &VanillaBlocks::GRAVEL, 6, 2,
             {"minecraft:dirt", "minecraft:coarse_dirt", "minecraft:grass_block"}, false},
            {"minecraft:clay_feature", &VanillaBlocks::CLAY, 4, 1,
             {"minecraft:dirt", "minecraft:coarse_dirt", "minecraft:mud"}, true}
    };

    static const SeagrassDefinition SEAGRASS[] = {
            {"minecraft:shortest_seagrass_feature", 1, 0.3f},
            {"minecraft:shorter_seagrass_feature", 1, 0.4f},
            {"minecraft:taller_seagrass_feature", 1, 0.8f},
            {"minecraft:seagrass_feature", 1, 0.6f}
    };

    // the game's definitions/features: tall_grass_feature and fern_feature place minecraft:tallgrass, upgraded to
    // these blocks
    static const SingleBlockDefinition SINGLE_BLOCKS[] = {
            {"minecraft:tall_grass_feature", &VanillaBlocks::SHORT_GRASS},
            {"minecraft:fern_feature", &VanillaBlocks::FERN}
    };

    static const auto single = [](size_t index) {
        Placed placed;
        placed.mSingleBlock = &SINGLE_BLOCKS[index];
        return placed;
    };
    static const auto seagrass = [](size_t index) {
        Placed placed;
        placed.mSeagrass = &SEAGRASS[index];
        return placed;
    };

    // BDS 1.26.61 behavior packs, features/select_taiga_plant_feature.json
    static const WeightedDefinition WEIGHTED[] = {
            {"minecraft:select_taiga_plant_feature", {{single(1), 4}, {single(0), 1}}}
    };

    static const auto weighted = [](size_t index) {
        Placed placed;
        placed.mWeighted = &WEIGHTED[index];
        return placed;
    };

    using AxisText = ScatterDefinition::AxisText;
    static const AxisText SEAGRASS_SIDE = {6, "-8", "8"};
    static const AxisText SEAGRASS_HEIGHT = {0, "query.heightmap(variable.worldx, variable.worldz)", ""};
    static const AxisText GRASS_SIDE = {6, "-7", "7"};
    static const AxisText GRASS_HEIGHT = {6, "-3", "3"};

    // BDS 1.26.61 behavior packs, features/scatter_*.json; eval order 4 is zxy, 5 zyx
    static const ScatterDefinition SCATTERS[] = {
            {"minecraft:scatter_cold_ocean_seagrass_feature", "32", {SEAGRASS_SIDE, SEAGRASS_HEIGHT, SEAGRASS_SIDE}, 4,
             seagrass(0)},
            {"minecraft:scatter_deep_cold_ocean_seagrass_feature", "40", {SEAGRASS_SIDE, SEAGRASS_HEIGHT, SEAGRASS_SIDE},
             4, seagrass(2)},
            {"minecraft:scatter_deep_ocean_seagrass_feature", "48", {SEAGRASS_SIDE, SEAGRASS_HEIGHT, SEAGRASS_SIDE}, 4,
             seagrass(2)},
            {"minecraft:scatter_deep_warm_ocean_seagrass_feature", "80", {SEAGRASS_SIDE, SEAGRASS_HEIGHT, SEAGRASS_SIDE},
             4, seagrass(2)},
            {"minecraft:scatter_ocean_seagrass_feature", "48", {SEAGRASS_SIDE, SEAGRASS_HEIGHT, SEAGRASS_SIDE}, 4,
             seagrass(0)},
            {"minecraft:scatter_river_seagrass_feature", "48", {SEAGRASS_SIDE, SEAGRASS_HEIGHT, SEAGRASS_SIDE}, 4,
             seagrass(1)},
            {"minecraft:scatter_swamp_seagrass_feature", "64", {SEAGRASS_SIDE, SEAGRASS_HEIGHT, SEAGRASS_SIDE}, 4,
             seagrass(3)},
            {"minecraft:scatter_warm_ocean_seagrass_feature", "80", {SEAGRASS_SIDE, SEAGRASS_HEIGHT, SEAGRASS_SIDE}, 4,
             seagrass(0)},
            {"minecraft:scatter_tall_grass_feature", "32", {GRASS_SIDE, GRASS_HEIGHT, GRASS_SIDE}, 5, single(0)},
            {"minecraft:scatter_meadow_tall_grass_feature", "16", {GRASS_SIDE, GRASS_HEIGHT, GRASS_SIDE}, 5, single(0)},
            {"minecraft:scatter_taiga_plant_feature", "32", {GRASS_SIDE, GRASS_HEIGHT, GRASS_SIDE}, 5, weighted(0)}
    };

    Feature feature;
    if (entry.mPass == AFTER_SURFACE_PASS) {
        // vtable 0x14F4A0F60, place sub_14D217680
        feature.mKelp = entry.mFeature == "minecraft:kelp_feature";
    } else if (entry.mPass == UNDERGROUND_PASS) {
        for (const OreDefinition &ore: ORES) {
            if (entry.mFeature == ore.mIdentifier)
                feature.mOre = &ore;
        }
    } else if (entry.mPass == SURFACE_PASS) {
        for (const DiskDefinition &disk: DISKS) {
            if (entry.mFeature == disk.mIdentifier)
                feature.mDisk = &disk;
        }
    }
    if (entry.mPass == SURFACE_PASS || entry.mPass == AFTER_SURFACE_PASS) {
        for (const ScatterDefinition &scatter: SCATTERS) {
            if (entry.mFeature == scatter.mIdentifier)
                feature.mPlaced.mScatter = &scatter;
        }
    }
    return feature;
}

bool OverworldRulePlacer::handles(const BiomeFeatureEntry &entry) {
    if (!_find(entry).isPorted() || entry.mEvalOrder < 0 || entry.mEvalOrder > 5)
        return false;

    if (FeatureExpression::parse(entry.mIterations) == nullptr)
        return false;
    if ((entry.mChanceNumerator <= 0 || entry.mChanceDenominator <= 0)
        && FeatureExpression::parse(entry.mChancePercent) == nullptr)
        return false;

    Axis axes[3];
    for (size_t axis = 0; axis < 3; axis++) {
        const BiomeFeatureEntry::Coordinate &coordinate = entry.mCoordinates[axis];
        if (!parseAxis(coordinate.mDistribution, coordinate.mMin, coordinate.mMax, axes[axis]))
            return false;
    }
    return drawnInOrder(axes, entry.mEvalOrder);
}

void OverworldRulePlacer::apply(BlockManager &blocks, const LevelChunk &chunk, uint32_t levelSeed,
                               const std::vector<int32_t> &biomes) {
    // the decoration seed of sub_14CDD4880, from the level seed and the chunk position
    std::mt19937 random(levelSeed);
    const uint32_t multiplierX = (random() >> 1) | 1u;
    const uint32_t multiplierZ = (random() >> 1) | 1u;
    const uint32_t decorationSeed = (multiplierZ * static_cast<uint32_t>(chunk.getZ())
                                     + multiplierX * static_cast<uint32_t>(chunk.getX())) ^ levelSeed;

    // sub_14CDDF960, sub_14A22E1E0, sub_14A22E470: each pass, every biome of the chunk, then its rules in order
    for (const char *pass: PASSES) {
        for (int32_t biome: biomes) {
            const std::vector<BiomeFeatureEntry> *rules = BiomeChunkGenDataRegistry::getConsolidatedFeatures(biome);
            if (rules == nullptr)
                continue;

            for (const BiomeFeatureEntry &entry: *rules) {
                if (entry.mPass != pass || !handles(entry))
                    continue;

                _placeRule(blocks, chunk, entry, _find(entry), decorationSeed, biomes.size() > 1 ? biome : -1);
            }
        }
    }
}

void OverworldRulePlacer::_placeRule(BlockManager &blocks, const LevelChunk &chunk, const BiomeFeatureEntry &entry,
                                     const Feature &feature, uint32_t decorationSeed, int32_t biomeFilter) {
    // sub_14A22D800: boost-style combine of the decoration seed with the rule's hash
    const uint32_t hash = static_cast<uint32_t>(hashedString(entry.mIdentifier));
    const uint32_t seed = decorationSeed ^ ((decorationSeed >> 2) + hash + (decorationSeed << 6) + 0x9E3779B9u);
    std::mt19937 scatter(seed);

    const int32_t origin[3] = {chunk.getX() << 4, 0, chunk.getZ() << 4};
    ScatterVariables variables(blocks, origin);
    int32_t iterations = iterationsOf(entry.mChanceNumerator, entry.mChanceDenominator,
                                      FeatureExpression::parse(entry.mChancePercent),
                                      FeatureExpression::parse(entry.mIterations), variables, scatter);
    if (iterations == 0)
        return;

    Axis axes[3];
    for (size_t axis = 0; axis < 3; axis++) {
        const BiomeFeatureEntry::Coordinate &coordinate = entry.mCoordinates[axis];
        parseAxis(coordinate.mDistribution, coordinate.mMin, coordinate.mMax, axes[axis]);
    }

    // the feature draws from a second twister on the same seed, shared by every iteration
    std::mt19937 placement(seed);
    while (iterations-- > 0) {
        int32_t position[3] = {0, 0, 0};
        scatterPosition(axes, entry.mEvalOrder, origin, variables, scatter, position);

        if (biomeFilter >= 0
            && static_cast<int32_t>(chunk.getBiomeAt(position[0] & 15, position[1], position[2] & 15)) != biomeFilter)
            continue;

        if (feature.mOre != nullptr)
            _placeOre(blocks, *feature.mOre, position[0], position[1], position[2], placement);
        else if (feature.mDisk != nullptr)
            _placeDisk(blocks, *feature.mDisk, position[0], position[1], position[2], placement);
        else if (feature.mPlaced.isSet())
            _place(blocks, feature.mPlaced, position[0], position[1], position[2], placement);
        else
            _placeKelp(blocks, position[0], position[1], position[2], placement);
    }
}

bool OverworldRulePlacer::_placeOre(BlockManager &blocks, const OreDefinition &ore, int32_t x, int32_t y, int32_t z,
                                    std::mt19937 &random) {
    // OreFeature::place (sub_14A2D6E50)
    const float size = static_cast<float>(ore.mCount);
    const float inverse = 1.0f / size;

    const float angle = (nextFloat(random) * PI) * 10430.378f;
    const float sine = sineAt(angle);
    const float cosine = sineAt(angle + 16384.0f);
    const float reach = size * 0.125f;
    const float alongX = reach * sine;
    const float alongZ = reach * cosine;
    const float originX = static_cast<float>(x);
    const float originZ = static_cast<float>(z);
    const float originY = static_cast<float>(y);

    const int32_t firstRoll = nextInt(random, 3);
    const float startX = (alongX + 8.0f) + originX;
    const float startZ = (alongZ + 8.0f) + originZ;
    const int32_t secondRoll = nextInt(random, 3);
    const float endX = (8.0f - alongX) + originX;
    const float endZ = (8.0f - alongZ) + originZ;
    const float endY = (static_cast<float>(secondRoll) + -2.0f) + originY;
    const float startY = (static_cast<float>(firstRoll) + -2.0f) + originY;

    const float deltaX = endX - startX;
    const float deltaY = endY - startY;
    const float deltaZ = endZ - startZ;
    const float length = std::sqrt(deltaZ * deltaZ + (deltaY * deltaY + deltaX * deltaX));
    const bool degenerate = length < 0.0001f;
    const float directionX = degenerate ? 0.0f : deltaX / length;
    const float directionZ = degenerate ? 0.0f : deltaZ / length;
    const float directionY = degenerate ? 0.0f : deltaY / length;
    const float horizontal = std::sqrt(directionZ * directionZ + directionX * directionX);
    const bool vertical = horizontal < 0.0001f;
    const float flatX = vertical ? 0.0f : directionX / horizontal;
    const float flatZ = vertical ? 0.0f : directionZ / horizontal;
    const float waveStep = PI * inverse;

    std::vector<Sphere> spheres((size_t) ore.mCount);
    for (int32_t i = 0; i < ore.mCount; i++) {
        const float step = static_cast<float>(i);
        Sphere &sphere = spheres[(size_t) i];
        sphere.mZ = (deltaZ * step) * inverse + startZ;
        sphere.mX = inverse * (deltaX * step) + startX;
        sphere.mY = inverse * (deltaY * step) + startY;
        const float roll = nextFloat(random);
        const float wave = sineAt((step * waveStep) * 10430.378f);
        const float radius = ((wave + 1.0f) * ((roll * size) * 0.0625f) + 1.0f) * 0.5f;
        sphere.mRadiusSquared = radius * radius;
        sphere.mRadius = std::sqrt(sphere.mRadiusSquared);
    }

    // spheres inside a later, larger one are dropped
    std::vector<const Sphere *> kept;
    float maxRadiusSquared = 0.0f;
    for (size_t i = 0; i < spheres.size(); i++) {
        const Sphere &sphere = spheres[i];
        bool inside = false;
        for (size_t j = i + 1; j < spheres.size() && !inside; j++) {
            const Sphere &other = spheres[j];
            if (!(sphere.mRadius <= other.mRadius))
                continue;

            const float dx = other.mX - sphere.mX;
            const float dy = other.mY - sphere.mY;
            const float dz = other.mZ - sphere.mZ;
            inside = other.mRadius > std::sqrt(dz * dz + (dy * dy + dx * dx)) + sphere.mRadius;
        }

        if (inside)
            continue;

        kept.push_back(&sphere);
        maxRadiusSquared = std::fmax(sphere.mRadiusSquared, maxRadiusSquared);
    }

    if (kept.empty())
        return false;

    int32_t minX = INT32_MAX;
    int32_t minY = INT32_MAX;
    int32_t minZ = INT32_MAX;
    int32_t maxX = INT32_MIN;
    int32_t maxY = INT32_MIN;
    int32_t maxZ = INT32_MIN;
    for (const Sphere *sphere: kept) {
        minX = std::min(minX, static_cast<int32_t>(std::floor(sphere->mX - sphere->mRadius)));
        minY = std::min(minY, static_cast<int32_t>(std::floor(sphere->mY - sphere->mRadius)));
        minZ = std::min(minZ, static_cast<int32_t>(std::floor(sphere->mZ - sphere->mRadius)));
        maxX = std::max(maxX, static_cast<int32_t>(std::floor(sphere->mRadius + sphere->mX)));
        maxY = std::max(maxY, static_cast<int32_t>(std::floor(sphere->mY + sphere->mRadius)));
        maxZ = std::max(maxZ, static_cast<int32_t>(std::floor(sphere->mZ + sphere->mRadius)));
    }

    std::vector<BlockState> states;
    for (const ReplaceRule &rule: ore.mRules)
        states.push_back(rule.mPlaces().toBlockState());

    bool placed = false;
    float centerX = static_cast<float>(minX) + 0.5f;
    for (int32_t blockX = minX; blockX <= maxX; blockX++, centerX += 1.0f) {
        const float flatAlongX = flatX * (centerX - startX);
        const float alongFromX = (centerX - startX) * directionX;
        float centerZ = static_cast<float>(minZ) + 0.5f;
        for (int32_t blockZ = minZ; blockZ <= maxZ; blockZ++, centerZ += 1.0f) {
            const float offsetZ = centerZ - startZ;
            const float projection = flatZ * offsetZ + flatAlongX;
            const float awayX = centerX - (projection * flatX + startX);
            const float awayZ = centerZ - (projection * flatZ + startZ);
            if (!(awayZ * awayZ + awayX * awayX <= maxRadiusSquared))
                continue;

            const float alongFromZ = offsetZ * directionZ;
            float centerY = static_cast<float>(minY) + 0.5f;
            for (int32_t blockY = minY; blockY <= maxY; blockY++, centerY += 1.0f) {
                const float along = ((centerY - startY) * directionY + alongFromX) + alongFromZ;
                const float lineX = centerX - (along * directionX + startX);
                const float lineZ = centerZ - (along * directionZ + startZ);
                const float lineY = centerY - (directionY * along + startY);
                if (lineZ * lineZ + (lineY * lineY + lineX * lineX) > maxRadiusSquared)
                    continue;

                bool inside = false;
                for (const Sphere *sphere: kept) {
                    const float dx = centerX - sphere->mX;
                    const float dz = centerZ - sphere->mZ;
                    const float dy = centerY - sphere->mY;
                    if (sphere->mRadiusSquared > dz * dz + (dy * dy + dx * dx)) {
                        inside = true;
                        break;
                    }
                }

                if (!inside)
                    continue;

                const BlockState &current = blocks.getBlockAt(blockX, blockY, blockZ);
                for (size_t index = 0; index < ore.mRules.size(); index++) {
                    if (!contains(ore.mRules[index].mMayReplace, current))
                        continue;

                    const BlockState &state = states[index];
                    if (current == state)
                        continue;

                    const float discard = ore.mDiscardChanceOnAirExposure;
                    if (discard > 0.0f && !(discard < 1.0f && nextFloat(random) >= discard)
                        && exposedToAir(blocks, blockX, blockY, blockZ))
                        continue;

                    blocks.setBlockStateAt(blockX, blockY, blockZ, state);
                    placed = true;
                    break;
                }
            }
        }
    }
    return placed;
}

bool OverworldRulePlacer::_placeDisk(BlockManager &blocks, const DiskDefinition &disk, int32_t x, int32_t y, int32_t z,
                                     std::mt19937 &random) {
    const BlockState state = disk.mPlaces().toBlockState();
    // only the material of the block the rule landed on is checked, water (type 5) on layer 0
    const BlockState &origin = blocks.getBlockAt(x, y, z);
    if (VanillaBlocks::getAs<WaterBlock>(origin.mName) == nullptr
        && !(disk.mStartsOnItself && origin.mName == state.mName))
        return false;

    const int32_t radius = nextInt(random, disk.mRadius - 2) + 2;
    const int32_t radiusSquared = radius * radius;
    for (int32_t blockX = x - radius; blockX <= x + radius; blockX++) {
        for (int32_t blockZ = z - radius; blockZ <= z + radius; blockZ++) {
            if ((blockX - x) * (blockX - x) + (blockZ - z) * (blockZ - z) > radiusSquared)
                continue;

            for (int32_t blockY = y - disk.mHalfHeight; blockY <= y + disk.mHalfHeight; blockY++) {
                if (disk.mReplaces.contains(blocks.getBlockAt(blockX, blockY, blockZ)))
                    blocks.setBlockStateAt(blockX, blockY, blockZ, state);
            }
        }
    }
    return true;
}

bool OverworldRulePlacer::_place(BlockManager &blocks, const Placed &placed, int32_t x, int32_t y, int32_t z,
                                 std::mt19937 &random) {
    if (placed.mSeagrass != nullptr)
        return _placeSeagrass(blocks, *placed.mSeagrass, x, y, z, random);
    if (placed.mScatter != nullptr)
        return _placeScatter(blocks, *placed.mScatter, x, y, z, random);
    if (placed.mSingleBlock != nullptr)
        return _placeSingleBlock(blocks, *placed.mSingleBlock, x, y, z, random);
    return _placeWeighted(blocks, *placed.mWeighted, x, y, z, random);
}

bool OverworldRulePlacer::_placeScatter(BlockManager &blocks, const ScatterDefinition &scatter, int32_t x, int32_t y,
                                        int32_t z, std::mt19937 &random) {
    // ScatterFeature::place (sub_14A255FC0): the iterations, positions and the placed feature all draw from the
    // rule's placement twister
    static const ExpressionPtr EVERY_TIME = FeatureExpression::parse("100");

    Axis axes[3];
    for (size_t axis = 0; axis < 3; axis++) {
        const ScatterDefinition::AxisText &text = scatter.mAxes[axis];
        parseAxis(text.mDistribution, text.mMin, text.mMax, axes[axis]);
    }

    const int32_t origin[3] = {x, y, z};
    ScatterVariables variables(blocks, origin);
    const int32_t iterations = iterationsOf(scatter.mChanceNumerator, scatter.mChanceDenominator, EVERY_TIME,
                                            FeatureExpression::parse(scatter.mIterations), variables, random);
    bool placed = false;
    for (int32_t iteration = 0; iteration < iterations; iteration++) {
        int32_t position[3] = {0, 0, 0};
        scatterPosition(axes, scatter.mEvalOrder, origin, variables, random, position);
        placed = _place(blocks, scatter.mPlaces, position[0], position[1], position[2], random) || placed;
    }
    return placed;
}

bool OverworldRulePlacer::_placeSingleBlock(BlockManager &blocks, const SingleBlockDefinition &single, int32_t x,
                                            int32_t y, int32_t z, std::mt19937 &random) {
    // SingleBlockFeature::place (sub_14A267E10): the block is drawn by weight even when it is the only one, then its
    // survival is enforced and only air is replaced; a short grass or fern survives on the blocks of PlantSupport::Dirt
    nextInt(random, 1);
    if (!IFeature::isSupportDirt(blocks.getBlockAt(x, y - 1, z)))
        return false;
    if (blocks.getBlockAt(x, y, z).mName != "minecraft:air")
        return false;

    blocks.setBlockStateAt(x, y, z, single.mBlock().toBlockState());
    return true;
}

bool OverworldRulePlacer::_placeWeighted(BlockManager &blocks, const WeightedDefinition &weighted, int32_t x,
                                         int32_t y, int32_t z, std::mt19937 &random) {
    // WeightedRandomFeature::place (sub_14A28A040)
    int32_t total = 0;
    for (const std::pair<Placed, int32_t> &feature: weighted.mFeatures)
        total += feature.second;

    int32_t roll = total != 0 ? nextInt(random, total) : 0;
    for (const std::pair<Placed, int32_t> &feature: weighted.mFeatures) {
        roll -= feature.second;
        if (roll < 0)
            return _place(blocks, feature.first, x, y, z, random);
    }
    return false;
}

bool OverworldRulePlacer::_placeSeagrass(BlockManager &blocks, const SeagrassDefinition &seagrass, int32_t x,
                                         int32_t y, int32_t z, std::mt19937 &random) {
    // SeagrassFeature::place (sub_1471E5DC0): down to the floor, then up through the water, a single or tall plant,
    // and a jitter around the floor block for the next one
    int32_t floorY = y;
    while (!isSolidBlocking(blocks, x, floorY, z) && floorY > LevelChunk::MIN_Y)
        floorY--;
    if (!isSolidBlocking(blocks, x, floorY, z))
        return false;

    // the temperature of the biome at the position it was given; cold water needs water above the plant
    LevelChunk *chunk = blocks.getLevel().peekChunkPtr(x >> 4, z >> 4);
    const int32_t biome = chunk != nullptr ? (int32_t) chunk->getBiomeAt(x & 15, y, z & 15) : -1;
    const bool cold = BiomeTemperature::forFeature(biome, x, y, z) < 0.15f;

    static const BlockState seagrassState = VanillaBlocks::SEAGRASS().toBlockState();
    static const BlockState lowerState = withState(seagrassState, "sea_grass_type", "double_bot");
    static const BlockState upperState = withState(seagrassState, "sea_grass_type", "double_top");

    int32_t plantX = x;
    int32_t plantY = floorY;
    int32_t plantZ = z;
    for (int32_t i = 0; i < seagrass.mCount; i++) {
        while (!isWater(blocks.getBlockAt(plantX, plantY, plantZ))
               && blocks.getBlockAt(plantX, plantY, plantZ).mName != "minecraft:air")
            plantY++;

        // SeagrassBlock::mayPlace (sub_14B2635C0): not on magma, soul sand or a fence, on a block with a solid top,
        // in a water source
        const BlockState &below = blocks.getBlockAt(plantX, plantY - 1, plantZ);
        const BlockState &at = blocks.getBlockAt(plantX, plantY, plantZ);
        const bool mayPlace = isWater(at) && WaterBlock::isSource(at) && !refusesWaterPlant(below)
                              && BlockSupport::isSolid(below);
        if (mayPlace) {
            if (seagrass.mShortChance <= nextFloat(random)) {
                if (!cold || isWater(blocks.getBlockAt(plantX, plantY + 1, plantZ)))
                    setInWater(blocks, plantX, plantY, plantZ, seagrassState);
            } else if (isWater(blocks.getBlockAt(plantX, plantY + 1, plantZ))
                       && (!cold || isWater(blocks.getBlockAt(plantX, plantY + 2, plantZ)))) {
                setInWater(blocks, plantX, plantY, plantZ, lowerState);
                setInWater(blocks, plantX, plantY + 1, plantZ, upperState);
            }
        }

        int32_t dx = 0;
        int32_t dz = 0;
        jitter(random, dx, dz);
        plantX = x + dx;
        plantY = floorY;
        plantZ = z + dz;
    }
    return true;
}

bool OverworldRulePlacer::_placeKelp(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random) {
    // KelpFeature::place (sub_14D217680), with its count of 1 for worlds of 1.21.60 and later
    const int32_t count = 1;
    // the values of kelp_age (qword_1529E3AA8), and the tallest water column measured
    const int32_t ageValues = 26;

    static const BlockState kelpState = VanillaBlocks::KELP().toBlockState();

    int32_t baseY = y;
    if (!isSolidBlocking(blocks, x, baseY - 1, z)) {
        do {
            if (blocks.getBlockAt(x, baseY, z).mName == kelpState.mName || baseY <= LevelChunk::MIN_Y)
                break;
            baseY--;
        } while (!isSolidBlocking(blocks, x, baseY - 1, z));
    }
    if (!isSolidBlocking(blocks, x, baseY - 1, z))
        return false;

    int32_t plantX = x;
    int32_t plantY = baseY;
    int32_t plantZ = z;
    for (int32_t i = 0; i < count; i++) {
        while (!isWater(blocks.getBlockAt(plantX, plantY, plantZ))
               && blocks.getBlockAt(plantX, plantY, plantZ).mName != "minecraft:air")
            plantY++;

        const BlockState &at = blocks.getBlockAt(plantX, plantY, plantZ);
        if (at.mName == "minecraft:air" || isWater(blocks.getBlockAt(plantX, plantY - 1, plantZ))
            || at.mName == kelpState.mName)
            continue;

        int32_t water = 0;
        while (water < ageValues && isWater(blocks.getBlockAt(plantX, plantY + water, plantZ)))
            water++;
        if (water < 3)
            continue;

        const int32_t length = water >= 4 ? nextInt(random, water - 3) + 3 : 3;
        const int32_t baseAge = ageValues - 1 - length;

        // KelpBlock::mayPlace (sub_143C82A70): not on magma, soul sand or a fence, in a water source, on kelp or a
        // solid block
        const BlockState &below = blocks.getBlockAt(plantX, plantY - 1, plantZ);
        const bool mayPlace = !refusesWaterPlant(below) && WaterBlock::isSource(at)
                              && (below.mName == kelpState.mName || isSolidBlocking(blocks, plantX, plantY - 1, plantZ));
        if (below.mName != kelpState.mName && mayPlace) {
            setInWater(blocks, plantX, plantY, plantZ, withState(kelpState, "kelp_age", baseAge));
            for (int32_t step = 1; step < length; step++) {
                if (!isWater(blocks.getBlockAt(plantX, plantY + step, plantZ)))
                    break;
                setInWater(blocks, plantX, plantY + step, plantZ, withState(kelpState, "kelp_age", baseAge + step));
            }
        }

        int32_t dx = 0;
        int32_t dz = 0;
        jitter(random, dx, dz);
        plantX = x + dx;
        plantY = baseY;
        plantZ = z + dz;
    }
    return true;
}
