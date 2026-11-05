#include "World/Level/LevelGen/Overworld/Feature/OverworldRulePlacer.h"
#include "World/Level/Block/Actor/BeehiveBlockActor.h"

#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/FenceBlock.h"
#include "World/Level/Block/FloweredAzaleaLeavesBlock.h"
#include "World/Level/Block/IceBlock.h"
#include "World/Level/Block/LavaBlock.h"
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
#include "World/Level/LevelGen/Feature/FeatureMath.h"
#include "World/Level/LevelGen/Feature/IFeature.h"
#include "World/Level/LevelGen/Feature/Tree/BeeNestGenerator.h"
#include "World/Level/LevelGen/Noise/NormalNoise.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"
#include "World/Level/LevelGen/Overworld/Feature/OverworldTreePlacer.h"
#include "World/Level/LevelGen/Overworld/Biome/BiomeTemperature.h"
#include "World/Level/LevelGen/Overworld/Surface/SurfaceSimplexNoise.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {
    constexpr float PI = 3.1415927f;
    constexpr const char *FIRST_PASS = "first_pass";
    constexpr const char *UNDERGROUND_PASS = "underground_pass";
    constexpr const char *SURFACE_PASS = "surface_pass";
    constexpr const char *AFTER_SURFACE_PASS = "after_surface_pass";

    // the passes the game places after the carvers, in order (sub_1471CE3F0)
    const char *const PASSES[] = {
            FIRST_PASS, "before_underground_pass", UNDERGROUND_PASS, "after_underground_pass",
            "before_surface_pass", SURFACE_PASS, "after_surface_pass", "before_sky_pass", "sky_pass",
            "after_sky_pass", "final_pass"
    };

    struct ReplaceRule {
        Block (*mPlaces)();
        std::vector<std::string> mMayReplace;
    };

    using FeatureRandom::between;
    using FeatureRandom::nextFloat;
    using FeatureRandom::nextInt;

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

    // uniform, gaussian and triangle are the only distributions the ported rules use
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
        return axis.mMax != nullptr && (distribution == 1 || distribution == 2 || distribution == 6);
    }

    // sub_1437FD570
    int32_t distribute(int32_t distribution, int32_t min, int32_t max, std::mt19937 &random) {
        if (distribution == 1)
            return nextInt(random, max - min) + min;
        if (distribution == 2) {
            // the difference of two raw draws modulo half the range, around its middle
            const int32_t half = (max - min) / 2;
            int32_t offset = 0;
            if (static_cast<uint32_t>(max - min + 1) >= 3u) {
                const auto first = static_cast<int32_t>(random() % static_cast<uint32_t>(half));
                offset = first - static_cast<int32_t>(random() % static_cast<uint32_t>(half));
            }
            return min + half + offset;
        }

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

        bool hasBiomeTag(const std::string &tag) const override {
            const auto x = static_cast<int32_t>(mOrigin[0]);
            const auto y = static_cast<int32_t>(mOrigin[1]);
            const auto z = static_cast<int32_t>(mOrigin[2]);
            LevelChunk *chunk = mBlocks.getLevel().peekChunkPtr(x >> 4, z >> 4);
            return chunk != nullptr
                   && BiomeChunkGenDataRegistry::hasTag((int32_t) chunk->getBiomeAt(x & 15, y, z & 15), tag);
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

    // the geode sums its distances with the bit trick and one Newton step
    float fastInverseSqrt(float value) {
        uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        bits = 0x5F3759DFu - (bits >> 1);
        float estimate = 0.0f;
        std::memcpy(&estimate, &bits, sizeof(estimate));
        return (((value * -0.5f) * estimate) * estimate + 1.5f) * estimate;
    }

    // a geode point may not land in air, a liquid, ice, bedrock, packed or blue ice (qword_1529B1E50,
    // qword_1529BAF38, qword_1529BAF08, materials of xmmword_152B18A30)
    bool refusesGeodePoint(const BlockState &state) {
        return state.mName == "minecraft:air" || state.mName == "minecraft:bedrock"
               || state.mName == "minecraft:packed_ice" || state.mName == "minecraft:blue_ice" || isWater(state)
               || LavaBlock::matches(state.mName) || IceBlock::matches(state.mName);
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
    // scatter_chance as a fraction, or as a percentage when it is 0
    int32_t mChanceNumerator = 0;
    int32_t mChanceDenominator = 0;
    const char *mChancePercent = "100";

    // parsed once with the definition, a placement only evaluates them
    ExpressionPtr mParsedIterations = FeatureExpression::parse(mIterations);
    ExpressionPtr mParsedChance = FeatureExpression::parse(mChancePercent);
    std::array<Axis, 3> mParsedAxes = _parseAxes(mAxes);

    static std::array<Axis, 3> _parseAxes(const AxisText (&texts)[3]) {
        std::array<Axis, 3> axes;
        for (size_t axis = 0; axis < 3; axis++)
            parseAxis(texts[axis].mDistribution, texts[axis].mMin, texts[axis].mMax, axes[axis]);
        return axes;
    }
};

// a minecraft:single_block_feature with one block, its survival enforced, replacing only air
struct OverworldRulePlacer::SingleBlockDefinition {
    const char *mIdentifier;
    Block (*mBlock)();
    // enforce_survivability_rules of a plant on the blocks of PlantSupport::Dirt
    bool mSurvival = true;
    // enforce_placement_rules of a double plant's lower half: a supporting block below and air above
    bool mDoublePlant = false;
    // upper_block_bit of a double plant's half, -1 for any other block
    int32_t mUpperBlockBit = -1;
    // may_replace short grass besides air
    bool mReplacesShortGrass = false;
};

// a minecraft:weighted_random_feature
struct OverworldRulePlacer::WeightedDefinition {
    const char *mIdentifier;
    std::vector<std::pair<Placed, int32_t>> mFeatures;
};

// a minecraft:aggregate_feature (place sub_14A2DDCA0): every feature at the same position, early_out 1 stopping
// while none has been placed and 2 at the first that is
struct OverworldRulePlacer::AggregateDefinition {
    const char *mIdentifier;
    std::vector<Placed> mFeatures;
    int32_t mEarlyOut;
};

// a minecraft:tree_feature, with the feature a fallen trunk places on its logs
struct OverworldRulePlacer::TreeDefinition {
    OverworldTreePlacer::Definition mTree;
    const OverworldTreePlacer::Fancy *mFancy;
    Placed mLogDecoration;
};

// one of the game's legacy tree features (LegacyTreeFeature, constructor sub_14D21F620, place sub_14D224440): a
// count of trees from the density, each feature picked by the type's own draws
struct OverworldRulePlacer::LegacyTreeDefinition {
    enum class Pick {
        // nextInt(5) != 0 ? (nextInt(10) != 0 ? third : second) : first
        Forest,
        First,
        // nextInt(3) == 0 ? first : second
        OneInThree
    };

    const char *mIdentifier;
    float mDensity;
    Pick mPick;
    std::vector<Placed> mFeatures;
};

// a minecraft:geode_feature (GeodeFeature, place sub_14D20B390)
struct OverworldRulePlacer::GeodeDefinition {
    const char *mIdentifier;
    Block (*mFiller)();
    Block (*mInnerLayer)();
    Block (*mAlternateInnerLayer)();
    Block (*mMiddleLayer)();
    Block (*mOuterLayer)();
    std::vector<Block (*)()> mInnerPlacements;
    int32_t mMinOuterWallDistance;
    int32_t mMaxOuterWallDistance;
    int32_t mMinDistributionPoints;
    int32_t mMaxDistributionPoints;
    int32_t mMinPointOffset;
    int32_t mMaxPointOffset;
    int32_t mMaxRadius;
    int32_t mCrackPointOffset;
    float mNoiseMultiplier;
    float mUsePotentialPlacementsChance;
    float mUseAlternateLayer0Chance;
    bool mPlacementsRequireLayer0Alternate;
    int32_t mInvalidBlocksThreshold;
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
            {"minecraft:fern_feature", &VanillaBlocks::FERN},
            {"minecraft:fern_double_plant_lower_feature", &VanillaBlocks::LARGE_FERN, false, true, 0},
            {"minecraft:fern_double_plant_upper_feature", &VanillaBlocks::LARGE_FERN, false, false, 1},
            {"minecraft:grass_double_plant_lower_feature", &VanillaBlocks::TALL_GRASS, false, true, 0, true},
            {"minecraft:grass_double_plant_upper_feature", &VanillaBlocks::TALL_GRASS, false, false, 1}
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

    static const auto scatter = [](const ScatterDefinition &definition) {
        Placed placed;
        placed.mScatter = &definition;
        return placed;
    };
    static const auto aggregate = [](const AggregateDefinition &definition) {
        Placed placed;
        placed.mAggregate = &definition;
        return placed;
    };
    static const auto tree = [](const TreeDefinition &definition) {
        Placed placed;
        placed.mTree = &definition;
        return placed;
    };
    static const auto mushroom = [](Block (*block)()) {
        Placed placed;
        placed.mMushroom = block;
        return placed;
    };
    static const Placed LEAF_LITTER = [] {
        Placed placed;
        placed.mLeafLitter = true;
        return placed;
    }();
    static const Placed BEEHIVE_SEARCH = [] {
        Placed placed;
        placed.mBeehiveSearch = true;
        return placed;
    }();

    // BDS 1.26.61 definitions/features and behavior packs, features/*.json
    static const AxisText ZERO = {0, "0", ""};
    static const ScatterDefinition LOG_BROWN_MUSHROOM = {
            "minecraft:optional_log_brown_mushroom_feature", "1", {ZERO, ZERO, ZERO}, 0,
            mushroom(&VanillaBlocks::BROWN_MUSHROOM), 0, 0, "50.0"};
    static const AggregateDefinition SELECT_LOG_MUSHROOM = {
            "minecraft:select_log_mushroom_feature",
            {scatter(LOG_BROWN_MUSHROOM), mushroom(&VanillaBlocks::RED_MUSHROOM)}, 2};
    static const ScatterDefinition LOG_MUSHROOMS = {
            "minecraft:optional_log_mushrooms_feature", "1", {ZERO, {0, "1", ""}, ZERO}, 0,
            aggregate(SELECT_LOG_MUSHROOM), 1, 10};

    using Tree = OverworldTreePlacer;
    static const Tree::Fancy FANCY_OAK = {5, 12, 0.618f, 1, 0.381f, 1.0f, 0.2f, 1.0f, 0.3f, 4, 3};
    static const TreeDefinition TREES[] = {
            {{"minecraft:oak_tree_feature", Tree::Trunk::Simple, &VanillaBlocks::OAK_LOG, {4, 7}, {0, 0},
              Tree::Canopy::Simple, &VanillaBlocks::OAK_LEAVES, {0, 0}, {0, 0}, {0, 0}, 0, 0.0f, false}, nullptr, {}},
            {{"minecraft:oak_tree_with_vines_feature", Tree::Trunk::Simple, &VanillaBlocks::OAK_LOG, {4, 7}, {0, 0},
              Tree::Canopy::Simple, &VanillaBlocks::OAK_LEAVES, {0, 0}, {0, 0}, {0, 0}, 0, 100.0f, false}, nullptr,
             {}},
            {{"minecraft:fancy_oak_tree_feature", Tree::Trunk::Fancy, &VanillaBlocks::OAK_LOG, {0, 0}, {0, 0},
              Tree::Canopy::Fancy, &VanillaBlocks::OAK_LEAVES, {0, 0}, {0, 0}, {0, 0}, 0, 0.0f, false}, &FANCY_OAK,
             {}},
            {{"minecraft:fallen_oak_tree_feature", Tree::Trunk::Fallen, &VanillaBlocks::OAK_LOG, {4, 7}, {0, 0},
              Tree::Canopy::None, nullptr, {0, 0}, {0, 0}, {0, 0}, 0, 75.0f, true}, nullptr, scatter(LOG_MUSHROOMS)},
            {{"minecraft:birch_tree_feature", Tree::Trunk::Simple, &VanillaBlocks::BIRCH_LOG, {5, 8}, {0, 0},
              Tree::Canopy::Simple, &VanillaBlocks::BIRCH_LEAVES, {0, 0}, {0, 0}, {0, 0}, 0, 0.0f, false}, nullptr,
             {}},
            {{"minecraft:fallen_birch_tree_feature", Tree::Trunk::Fallen, &VanillaBlocks::BIRCH_LOG, {5, 8}, {0, 0},
              Tree::Canopy::None, nullptr, {0, 0}, {0, 0}, {0, 0}, 0, 0.0f, true}, nullptr, scatter(LOG_MUSHROOMS)},
            {{"minecraft:spruce_tree_feature", Tree::Trunk::Simple, &VanillaBlocks::SPRUCE_LOG, {6, 10}, {-2, 1},
              Tree::Canopy::Spruce, &VanillaBlocks::SPRUCE_LEAVES, {1, 3}, {0, 3}, {2, 4}, 0, 0.0f, false}, nullptr,
             {}},
            {{"minecraft:spruce_tree_with_vines_feature", Tree::Trunk::Simple, &VanillaBlocks::SPRUCE_LOG, {6, 10},
              {-2, 1}, Tree::Canopy::Spruce, &VanillaBlocks::SPRUCE_LEAVES, {1, 3}, {0, 3}, {2, 4}, 0, 100.0f, false},
             nullptr, {}},
            {{"minecraft:fallen_spruce_tree_feature", Tree::Trunk::Fallen, &VanillaBlocks::SPRUCE_LOG, {6, 10},
              {0, 0}, Tree::Canopy::None, nullptr, {0, 0}, {0, 0}, {0, 0}, 0, 0.0f, true}, nullptr,
             scatter(LOG_MUSHROOMS)},
            // the pine canopy height takes the upper offset's place
            {{"minecraft:pine_tree_feature", Tree::Trunk::Simple, &VanillaBlocks::SPRUCE_LOG, {7, 12}, {0, 0},
              Tree::Canopy::Pine, &VanillaBlocks::SPRUCE_LEAVES, {0, 0}, {3, 5}, {0, 0}, 1, 0.0f, false}, nullptr, {}}
    };

    static const auto optional = [](const char *identifier, const Placed &places, int32_t denominator) {
        return ScatterDefinition{identifier, "1", {ZERO, ZERO, ZERO}, 0, places, 1, denominator};
    };
    static const ScatterDefinition OPTIONAL_TREES[] = {
            optional("minecraft:optional_fallen_oak_tree_feature", tree(TREES[3]), 80),
            optional("minecraft:optional_oak_tree_with_vines_feature", tree(TREES[1]), 12),
            optional("minecraft:optional_fallen_birch_tree_feature", tree(TREES[5]), 80),
            optional("minecraft:optional_fallen_spruce_tree_feature", tree(TREES[8]), 80),
            optional("minecraft:optional_spruce_tree_with_vines_feature", tree(TREES[7]), 12)
    };
    static const ScatterDefinition OPTIONAL_BEEHIVE = {
            "minecraft:optional_beehive_feature", "1", {ZERO, ZERO, ZERO}, 0, BEEHIVE_SEARCH, 0, 0,
            "query.has_biome_tag('bee_habitat') ? (query.has_biome_tag('plains') || "
            "query.has_biome_tag('sunflower_plains') ? 5.0 : (query.has_biome_tag('flower_forest') ? 3.0 : "
            "(query.has_biome_tag('birch_forest') || query.has_biome_tag('birch_forest_hills') || "
            "query.has_biome_tag('birch_forest_mutated') || query.has_biome_tag('forest') || "
            "query.has_biome_tag('forest_mutated') ? 0.035 : 0.0))) : 0.0"};
    static const ScatterDefinition LEAF_LITTER_SCATTER = {
            "minecraft:scatter_leaf_litter_feature", "96", {{2, "-4", "4"}, {2, "-2", "2"}, {2, "-4", "4"}}, 4,
            LEAF_LITTER};
    static const AggregateDefinition OAK_WITH_BEEHIVE = {
            "minecraft:oak_tree_with_optional_beehive_feature", {tree(TREES[0]), scatter(OPTIONAL_BEEHIVE)}, 1};
    static const AggregateDefinition STANDING_OAK = {
            "minecraft:select_standing_oak_tree_feature", {scatter(OPTIONAL_TREES[1]), aggregate(OAK_WITH_BEEHIVE)}, 2};
    static const AggregateDefinition SELECT_OAK = {
            "minecraft:select_oak_tree_feature", {scatter(OPTIONAL_TREES[0]), aggregate(STANDING_OAK)}, 2};
    static const AggregateDefinition FANCY_WITH_BEEHIVE = {
            "minecraft:fancy_oak_tree_with_optional_beehive_feature", {tree(TREES[2]), scatter(OPTIONAL_BEEHIVE)}, 1};
    static const AggregateDefinition BIRCH_WITH_BEEHIVE = {
            "minecraft:birch_tree_with_optional_beehive_feature", {tree(TREES[4]), scatter(OPTIONAL_BEEHIVE)}, 1};
    static const AggregateDefinition SELECT_BIRCH = {
            "minecraft:select_birch_tree_feature", {scatter(OPTIONAL_TREES[2]), aggregate(BIRCH_WITH_BEEHIVE)}, 2};
    static const AggregateDefinition STANDING_SPRUCE = {
            "minecraft:select_standing_spruce_tree_feature", {scatter(OPTIONAL_TREES[4]), tree(TREES[6])}, 2};
    static const AggregateDefinition SELECT_SPRUCE = {
            "minecraft:select_spruce_tree_feature", {scatter(OPTIONAL_TREES[3]), aggregate(STANDING_SPRUCE)}, 2};
    static const AggregateDefinition WITH_LEAF_LITTER[] = {
            {"minecraft:select_birch_tree_with_leaf_litter_feature",
             {aggregate(SELECT_BIRCH), scatter(LEAF_LITTER_SCATTER)}, 1},
            {"minecraft:fancy_oak_tree_with_leaf_litter_and_optional_beehive",
             {aggregate(FANCY_WITH_BEEHIVE), scatter(LEAF_LITTER_SCATTER)}, 1},
            {"minecraft:select_oak_tree_with_leaf_litter_feature",
             {aggregate(SELECT_OAK), scatter(LEAF_LITTER_SCATTER)}, 1}
    };

    using Pick = LegacyTreeDefinition::Pick;
    static const LegacyTreeDefinition LEGACY_TREES[] = {
            {"minecraft:legacy:forest_tree_feature", 10.0f, Pick::Forest,
             {aggregate(WITH_LEAF_LITTER[0]), aggregate(WITH_LEAF_LITTER[1]), aggregate(WITH_LEAF_LITTER[2])}},
            {"minecraft:legacy:ice_tree_feature", 0.1f, Pick::First, {aggregate(SELECT_SPRUCE)}},
            {"minecraft:legacy:plains_tree_feature", 0.05f, Pick::OneInThree,
             {aggregate(FANCY_WITH_BEEHIVE), aggregate(SELECT_OAK)}},
            {"minecraft:legacy:taiga_tree_feature", 10.0f, Pick::OneInThree,
             {tree(TREES[9]), aggregate(SELECT_SPRUCE)}}
    };

    // the double plant patches: a lower half, then the upper one placed one block above it
    static const AxisText UP = {0, "1", ""};
    static const ScatterDefinition UPPER_HALVES[] = {
            {"minecraft:fixup_fern_double_plant_upper_position_feature", "1", {ZERO, UP, ZERO}, 0, single(3)},
            {"minecraft:fixup_grass_double_plant_upper_position_feature", "1", {ZERO, UP, ZERO}, 0, single(5)}
    };
    static const AggregateDefinition DOUBLE_PLANTS[] = {
            {"minecraft:fern_double_plant_feature", {single(2), scatter(UPPER_HALVES[0])}, 1},
            {"minecraft:grass_double_plant_feature", {single(4), scatter(UPPER_HALVES[1])}, 1}
    };
    static const ScatterDefinition DOUBLE_PLANT_PATCHES[] = {
            {"minecraft:fern_double_plant_patch_feature", "64", {{6, "-8", "8"}, {6, "-4", "4"}, {6, "-8", "8"}}, 5,
             aggregate(DOUBLE_PLANTS[0])},
            {"minecraft:grass_double_plant_patch_feature", "32", {{6, "-7", "7"}, {6, "-3", "3"}, {6, "-7", "7"}}, 5,
             aggregate(DOUBLE_PLANTS[1])}
    };

    // BDS 1.26.61 behavior packs, features/amethyst_geode_feature.json
    static const GeodeDefinition GEODES[] = {
            {"minecraft:amethyst_geode_feature", &VanillaBlocks::AIR, &VanillaBlocks::AMETHYST_BLOCK,
             &VanillaBlocks::BUDDING_AMETHYST, &VanillaBlocks::CALCITE, &VanillaBlocks::SMOOTH_BASALT,
             {&VanillaBlocks::AMETHYST_CLUSTER, &VanillaBlocks::LARGE_AMETHYST_BUD, &VanillaBlocks::MEDIUM_AMETHYST_BUD,
              &VanillaBlocks::SMALL_AMETHYST_BUD},
             4, 7, 3, 5, 1, 3, 16, 2, 0.025f, 0.35f, 0.083f, true, 1}
    };

    Feature feature;
    if (entry.mPass == AFTER_SURFACE_PASS) {
        // vtable 0x14F4A0F60, place sub_14D217680
        feature.mKelp = entry.mFeature == "minecraft:kelp_feature";
        for (const GeodeDefinition &geode: GEODES) {
            if (entry.mFeature == geode.mIdentifier)
                feature.mGeode = &geode;
        }
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
        for (const LegacyTreeDefinition &trees: LEGACY_TREES) {
            if (entry.mFeature == trees.mIdentifier)
                feature.mLegacyTree = &trees;
        }
    }
    if (entry.mPass == FIRST_PASS)
        feature.mForestFoliage = entry.mFeature == "minecraft:legacy:forest_foliage_feature";
    if (entry.mPass == FIRST_PASS || entry.mPass == SURFACE_PASS || entry.mPass == AFTER_SURFACE_PASS) {
        for (const ScatterDefinition &patch: DOUBLE_PLANT_PATCHES) {
            if (entry.mFeature == patch.mIdentifier)
                feature.mPlaced.mScatter = &patch;
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
        else if (feature.mGeode != nullptr)
            _placeGeode(blocks, *feature.mGeode, position[0], position[1], position[2], placement);
        else if (feature.mForestFoliage)
            _placeForestFoliage(blocks, position[0], position[2], placement);
        else if (feature.mLegacyTree != nullptr)
            _placeLegacyTrees(blocks, *feature.mLegacyTree, position[0], position[1], position[2], placement);
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
    const float sine = FeatureMath::sineAt(angle);
    const float cosine = FeatureMath::sineAt(angle + 16384.0f);
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
        const float wave = FeatureMath::sineAt((step * waveStep) * 10430.378f);
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
    if (placed.mWeighted != nullptr)
        return _placeWeighted(blocks, *placed.mWeighted, x, y, z, random);
    if (placed.mAggregate != nullptr)
        return _placeAggregate(blocks, *placed.mAggregate, x, y, z, random);
    if (placed.mTree != nullptr)
        return _placeTree(blocks, *placed.mTree, x, y, z, random);
    if (placed.mLeafLitter)
        return _placeLeafLitter(blocks, x, y, z, random);
    if (placed.mMushroom != nullptr)
        return _placeMushroom(blocks, placed.mMushroom, x, y, z);
    return _placeBeehiveSearch(blocks, x, y, z, random);
}

bool OverworldRulePlacer::_placeScatter(BlockManager &blocks, const ScatterDefinition &scatter, int32_t x, int32_t y,
                                        int32_t z, std::mt19937 &random) {
    // ScatterFeature::place (sub_14A255FC0): the iterations, positions and the placed feature all draw from the
    // rule's placement twister
    const int32_t origin[3] = {x, y, z};
    ScatterVariables variables(blocks, origin);
    const int32_t iterations = iterationsOf(scatter.mChanceNumerator, scatter.mChanceDenominator,
                                            scatter.mParsedChance, scatter.mParsedIterations, variables, random);
    bool placed = false;
    for (int32_t iteration = 0; iteration < iterations; iteration++) {
        int32_t position[3] = {0, 0, 0};
        scatterPosition(scatter.mParsedAxes.data(), scatter.mEvalOrder, origin, variables, random, position);
        placed = _place(blocks, scatter.mPlaces, position[0], position[1], position[2], random) || placed;
    }
    return placed;
}

bool OverworldRulePlacer::_placeSingleBlock(BlockManager &blocks, const SingleBlockDefinition &single, int32_t x,
                                            int32_t y, int32_t z, std::mt19937 &random) {
    // SingleBlockFeature::place (sub_14A267E10): the block is drawn by weight even when it is the only one, then its
    // survival is enforced and only air is replaced; a short grass or fern survives on the blocks of PlantSupport::Dirt
    nextInt(random, 1);
    const bool supported = IFeature::isSupportDirt(blocks.getBlockAt(x, y - 1, z));
    if (single.mSurvival && !supported)
        return false;
    if (single.mDoublePlant && (!supported || blocks.getBlockAt(x, y + 1, z).mName != "minecraft:air"))
        return false;

    const std::string &replaced = blocks.getBlockAt(x, y, z).mName;
    if (replaced != "minecraft:air" && !(single.mReplacesShortGrass && replaced == "minecraft:short_grass"))
        return false;

    const BlockState state = single.mBlock().toBlockState();
    if (single.mUpperBlockBit < 0) {
        blocks.setBlockStateAt(x, y, z, state);
        return true;
    }

    Tag states = state.mStates;
    states.putByte("upper_block_bit", static_cast<int8_t>(single.mUpperBlockBit));
    blocks.setBlockStateAt(x, y, z, BlockState(state.mName, states));
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

bool OverworldRulePlacer::_placeGeode(BlockManager &blocks, const GeodeDefinition &geode, int32_t x, int32_t y,
                                      int32_t z, std::mt19937 &random) {
    // GeodeFeature::place (sub_14D20B390); the structure overlap test that comes first has no counterpart here
    const int32_t pointCount = between(random, geode.mMinDistributionPoints, geode.mMaxDistributionPoints);
    const NormalNoise noise(random, -4, {1.0f});

    // the layer thresholds of sub_14D20B390: the filling one is the constant 1 / sqrt(1.7)
    const float share = static_cast<float>(pointCount) / static_cast<float>(geode.mMaxOuterWallDistance);
    const float filling = 0x1.88afa2p-1f;
    const float inner = 1.0f / std::sqrt(share + 2.2f);
    const float middle = 1.0f / std::sqrt(share + 3.2f);
    const float outer = 1.0f / std::sqrt(share + 4.2f);
    // the game reads neither generate_crack_chance nor base_crack_size and uses these constants
    const float crackBase = nextFloat(random) * 0.5f + 2.0f;
    const float crackSize = 1.0f / std::sqrt((pointCount >= 4 ? share : 0.0f) + crackBase);
    const bool crack = nextFloat(random) < 0.95f;

    struct Point {
        int32_t mX;
        int32_t mY;
        int32_t mZ;
        int32_t mOffset;
    };
    std::vector<Point> points;
    int32_t invalid = 0;
    for (int32_t i = 0; i < pointCount; i++) {
        const int32_t pointX = x + between(random, geode.mMinOuterWallDistance, geode.mMaxOuterWallDistance);
        const int32_t pointY = y + between(random, geode.mMinOuterWallDistance, geode.mMaxOuterWallDistance);
        const int32_t pointZ = z + between(random, geode.mMinOuterWallDistance, geode.mMaxOuterWallDistance);

        // the block and its liquid, which is the block itself when the second layer is empty
        const BlockState &extra = blocks.getExtraBlockAt(pointX, pointY, pointZ);
        if (refusesGeodePoint(blocks.getBlockAt(pointX, pointY, pointZ))
            || (extra.mName != "minecraft:air" && refusesGeodePoint(extra))) {
            if (invalid >= geode.mInvalidBlocksThreshold)
                return false;
            invalid++;
        }

        points.push_back({pointX, pointY, pointZ, between(random, geode.mMinPointOffset, geode.mMaxPointOffset)});
    }

    std::vector<std::array<int32_t, 3>> crackPoints;
    if (crack) {
        const int32_t side = 2 * pointCount + 1;
        const int32_t direction = nextInt(random, 4);
        const int32_t crackX = direction == 0 || direction == 2 ? side : 0;
        const int32_t crackZ = direction == 1 || direction == 2 ? side : 0;
        for (int32_t height: {7, 5, 1})
            crackPoints.push_back({x + crackX, y + height, z + crackZ});
    }

    static const BlockState airState = VanillaBlocks::AIR().toBlockState();
    const BlockState fillerState = geode.mFiller().toBlockState();
    const BlockState innerState = geode.mInnerLayer().toBlockState();
    const BlockState alternateState = geode.mAlternateInnerLayer().toBlockState();
    const BlockState middleState = geode.mMiddleLayer().toBlockState();
    const BlockState outerState = geode.mOuterLayer().toBlockState();

    // a layer is set once the scan ends; the crack's air is set at once
    std::vector<std::pair<std::array<int32_t, 3>, const BlockState *>> layers;
    std::vector<std::array<int32_t, 3>> potentials;
    bool placed = false;

    // the second layer is emptied first, the block kept when the tag protects it
    const auto setNow = [&](int32_t blockX, int32_t blockY, int32_t blockZ, const BlockState &state) {
        if (blockY < LevelChunk::MIN_Y || blockY > LevelChunk::MAX_Y)
            return false;
        if (blocks.getExtraBlockAt(blockX, blockY, blockZ).mName != "minecraft:air")
            blocks.setExtraBlockAt(blockX, blockY, blockZ, airState);
        if (DecorationSupport::isNotFeatureReplaceable(blocks.getBlockAt(blockX, blockY, blockZ)))
            return false;

        blocks.setBlockStateAt(blockX, blockY, blockZ, state);
        return true;
    };

    // one row along z (sub_14D20D740); a row past the shape stops its column once the geode has placed a block
    const auto row = [&](int32_t blockX, int32_t blockY, bool &columnPlaced, bool &placedBefore) {
        bool placedHere = false;
        for (int32_t blockZ = z - geode.mMaxRadius; blockZ <= z + geode.mMaxRadius; blockZ++) {
            const float value = noise.getValue(blockX, blockY, blockZ);

            float distance = 0.0f;
            for (const Point &point: points) {
                const int64_t dx = (int64_t) blockX - point.mX;
                const int64_t dy = (int64_t) blockY - point.mY;
                const int64_t dz = (int64_t) blockZ - point.mZ;
                distance += fastInverseSqrt(static_cast<float>(point.mOffset)
                                            + static_cast<float>(dx * dx + dy * dy + dz * dz));
            }

            const float shift = value * geode.mNoiseMultiplier;
            const float density = static_cast<float>(pointCount) * shift + distance;
            if (density < outer)
                continue;

            float crackDistance = 0.0f;
            for (const std::array<int32_t, 3> &point: crackPoints) {
                const int64_t dx = (int64_t) blockX - point[0];
                const int64_t dy = (int64_t) blockY - point[1];
                const int64_t dz = (int64_t) blockZ - point[2];
                crackDistance += fastInverseSqrt(static_cast<float>(dx * dx + dy * dy + dz * dz)
                                                 + static_cast<float>(geode.mCrackPointOffset));
            }

            const std::array<int32_t, 3> position = {blockX, blockY, blockZ};
            if (crack && crackDistance + shift * static_cast<float>(crackPoints.size()) >= crackSize
                && filling > density) {
                placed = setNow(blockX, blockY, blockZ, airState) || placed;
            } else if (density >= filling) {
                layers.emplace_back(position, &fillerState);
            } else if (density >= inner) {
                const bool alternate = geode.mUseAlternateLayer0Chance > nextFloat(random);
                layers.emplace_back(position, alternate ? &alternateState : &innerState);
                if ((alternate || !geode.mPlacementsRequireLayer0Alternate)
                    && geode.mUsePotentialPlacementsChance > nextFloat(random))
                    potentials.push_back(position);
            } else if (density >= middle) {
                layers.emplace_back(position, &middleState);
            } else {
                layers.emplace_back(position, &outerState);
            }

            if (placed) {
                placedBefore = true;
                placedHere = true;
            }
        }

        if (!placedBefore && !placedHere)
            return false;

        columnPlaced = true;
        return placedBefore && !placedHere;
    };

    // the columns from the origin outwards, east then west, up then down in each
    bool stopped = false;
    const auto column = [&](int32_t blockX) {
        bool columnPlaced = false;
        bool placedUp = false;
        for (int32_t blockY = y; blockY <= y + geode.mMaxRadius; blockY++) {
            if (row(blockX, blockY, columnPlaced, placedUp))
                break;
        }

        bool placedDown = false;
        for (int32_t blockY = y - 1; blockY >= y - geode.mMaxRadius; blockY--) {
            if (row(blockX, blockY, columnPlaced, placedDown))
                break;
        }

        stopped = stopped || placedUp || placedDown;
        return !columnPlaced && stopped;
    };

    for (int32_t blockX = x; blockX <= x + geode.mMaxRadius; blockX++) {
        if (column(blockX))
            break;
    }
    for (int32_t blockX = x - 1; blockX >= x - geode.mMaxRadius; blockX--) {
        if (column(blockX))
            break;
    }

    for (const std::pair<std::array<int32_t, 3>, const BlockState *> &layer: layers) {
        const std::array<int32_t, 3> &position = layer.first;
        placed = setNow(position[0], position[1], position[2], *layer.second) || placed;
    }

    // a bud on the first open face, up, down, north, south, west then east (qword_14F1A9250)
    static const int32_t FACES[6][3] = {{0, 1, 0}, {0, -1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};
    static const char *const FACE_NAMES[6] = {"up", "down", "north", "south", "west", "east"};
    if (geode.mInnerPlacements.empty())
        return placed;

    for (const std::array<int32_t, 3> &position: potentials) {
        const BlockState bud = geode.mInnerPlacements[(size_t) nextInt(
                random, (int32_t) geode.mInnerPlacements.size())]().toBlockState();
        for (size_t face = 0; face < 6; face++) {
            const int32_t budX = position[0] + FACES[face][0];
            const int32_t budY = position[1] + FACES[face][1];
            const int32_t budZ = position[2] + FACES[face][2];
            const BlockState &replaced = blocks.getBlockAt(budX, budY, budZ);
            // the bud stands on the block it grows from, the geode's own layer
            if ((replaced.mName != "minecraft:air" && !isWater(replaced))
                || !BlockSupport::isSolid(blocks.getBlockAt(position[0], position[1], position[2])))
                continue;

            placed = setNow(budX, budY, budZ, withState(bud, "minecraft:block_face", FACE_NAMES[face])) || placed;
            break;
        }
    }
    return placed;
}

void OverworldRulePlacer::_placeLegacyTrees(BlockManager &blocks, const LegacyTreeDefinition &trees, int32_t x,
                                            int32_t y, int32_t z, std::mt19937 &random) {
    // LegacyTreeFeature::place (sub_14D224440): nine times in ten the density plus up to 2 more, or for a density
    // below 1 one tree with that chance; otherwise the density scaled by a float
    (void) y;
    const float density = trees.mDensity;
    int32_t count = 0;
    if (nextInt(random, 10) != 0) {
        if (density >= 1.0f) {
            const int32_t extra = nextInt(random, 10);
            count = static_cast<int32_t>(density) + (extra >= 8 ? extra - 7 : 0);
        } else {
            count = density > nextFloat(random) ? 1 : 0;
        }
    } else {
        count = static_cast<int32_t>(density * nextFloat(random));
    }

    // sub_14D21F430: the feature, then z and x inside the chunk's border and the tree on the generation heightmap;
    // the grass it places around each tree is skipped, sub_145E2AA10 reading true for these worlds
    using Pick = LegacyTreeDefinition::Pick;
    for (int32_t tree = 0; tree < count; tree++) {
        size_t picked = 0;
        if (trees.mPick == Pick::Forest)
            picked = nextInt(random, 5) != 0 ? (nextInt(random, 10) != 0 ? 2 : 1) : 0;
        else if (trees.mPick == Pick::OneInThree)
            picked = nextInt(random, 3) == 0 ? 0 : 1;

        const int32_t treeZ = z + nextInt(random, 14) + 1;
        const int32_t treeX = x + nextInt(random, 14) + 1;
        const int32_t treeY = heightmapAt(blocks.getLevel(), treeX, treeZ);
        _place(blocks, trees.mFeatures[picked], treeX, treeY, treeZ, random);
    }
}

bool OverworldRulePlacer::_placeAggregate(BlockManager &blocks, const AggregateDefinition &aggregate, int32_t x,
                                          int32_t y, int32_t z, std::mt19937 &random) {
    bool placed = false;
    for (const Placed &feature: aggregate.mFeatures) {
        if (_place(blocks, feature, x, y, z, random)) {
            placed = true;
            if (aggregate.mEarlyOut == 2)
                return true;
        }
        if (aggregate.mEarlyOut == 1 && !placed)
            break;
    }
    return placed;
}

bool OverworldRulePlacer::_placeTree(BlockManager &blocks, const TreeDefinition &tree, int32_t x, int32_t y,
                                     int32_t z, std::mt19937 &random) {
    return OverworldTreePlacer::place(blocks, tree.mTree, tree.mFancy, x, y, z, random,
                                      [&](int32_t logX, int32_t logY, int32_t logZ) {
                                          _place(blocks, tree.mLogDecoration, logX, logY, logZ, random);
                                      });
}

bool OverworldRulePlacer::_placeLeafLitter(BlockManager &blocks, int32_t x, int32_t y, int32_t z,
                                           std::mt19937 &random) {
    // the game's leaf litter feature (vtable 0x14F4A0ED0, sub_14D209540): growth and cardinal direction drawn
    // first, then placed in air where the block may stand
    static const char *const DIRECTIONS[4] = {"south", "west", "north", "east"};
    const int32_t growth = nextInt(random, 4);
    const int32_t direction = nextInt(random, 4);
    if (blocks.getBlockAt(x, y, z).mName != "minecraft:air" || !BlockSupport::isSolid(blocks.getBlockAt(x, y - 1, z)))
        return false;

    const BlockState litter = withState(withState(VanillaBlocks::LEAF_LITTER().toBlockState(), "growth", growth),
                                        "minecraft:cardinal_direction", DIRECTIONS[direction]);
    blocks.setBlockStateAt(x, y, z, litter);
    return true;
}

bool OverworldRulePlacer::_placeMushroom(BlockManager &blocks, Block (*mushroom)(), int32_t x, int32_t y, int32_t z) {
    if (blocks.getBlockAt(x, y, z).mName != "minecraft:air" || !BlockSupport::isSolid(blocks.getBlockAt(x, y - 1, z)))
        return false;

    blocks.setBlockStateAt(x, y, z, mushroom().toBlockState());
    return true;
}

bool OverworldRulePlacer::_placeBeehiveSearch(BlockManager &blocks, int32_t x, int32_t y, int32_t z,
                                              std::mt19937 &random) {
    // beehive_search_feature: up to 12 blocks up the trunk, select_beehive_feature picking one of three side orders,
    // each side a beehive_feature that draws its block before checking where it attaches
    static const int32_t EAST[2] = {1, 0};
    static const int32_t SOUTH[2] = {0, 1};
    static const int32_t WEST[2] = {-1, 0};
    static const int32_t *const ORDERS[3][3] = {{EAST, SOUTH, WEST}, {SOUTH, WEST, EAST}, {WEST, EAST, SOUTH}};
    const auto isTrunk = [&](int32_t blockX, int32_t blockY, int32_t blockZ) {
        const std::string &name = blocks.getBlockAt(blockX, blockY, blockZ).mName;
        return name == "minecraft:oak_log" || name == "minecraft:birch_log" || name == "minecraft:mangrove_roots"
               || name == "minecraft:cherry_log";
    };
    for (int32_t offsetY = 0; offsetY <= 12; offsetY++) {
        const int32_t order = nextInt(random, 3);
        for (const int32_t *side: ORDERS[order]) {
            nextInt(random, 1);
            const int32_t nestX = x + side[0];
            const int32_t nestY = y + offsetY;
            const int32_t nestZ = z + side[1];
            if (blocks.getBlockAt(nestX, nestY, nestZ).mName != "minecraft:air"
                || blocks.getBlockAt(nestX, nestY, nestZ + 1).mName != "minecraft:air")
                continue;

            const std::string &top = blocks.getBlockAt(nestX, nestY + 1, nestZ).mName;
            const int32_t attached = (top == "minecraft:oak_leaves" || top == "minecraft:birch_leaves"
                                      || top == "minecraft:mangrove_leaves" || top == "minecraft:cherry_leaves")
                                     + isTrunk(nestX - 1, nestY, nestZ) + isTrunk(nestX, nestY, nestZ - 1)
                                     + isTrunk(nestX + 1, nestY, nestZ);
            if (attached < 2)
                continue;

            // a south-facing nest with its bees, as the mangroves place theirs
            BeeNestGenerator::placeAt(blocks, nestX, nestY, nestZ, BeehiveBlockActor::MAX_OCCUPANTS);
            return true;
        }
    }
    return false;
}

void OverworldRulePlacer::_placeForestFoliage(BlockManager &blocks, int32_t x, int32_t z, std::mt19937 &random) {
    // LegacyForestFoliageFeature::place (vtable 0x14F4A14C0, sub_14D21D0B0) with the forest's type 1: nextInt(5) - 3
    // groups of one double plant, each tried five times at a column of the chunk and a height below its heightmap
    // plus 32
    static Block (*const PLANTS[3])() = {&VanillaBlocks::LILAC, &VanillaBlocks::ROSE_BUSH, &VanillaBlocks::PEONY};
    for (int32_t group = nextInt(random, 5) - 3; group > 0; group--) {
        Block (*const plant)() = PLANTS[nextInt(random, 3)];
        for (int32_t attempt = 0; attempt < 5; attempt++) {
            const int32_t plantX = x + nextInt(random, 16);
            const int32_t plantZ = z + nextInt(random, 16);
            const auto limit = static_cast<int16_t>(heightmapAt(blocks.getLevel(), plantX, plantZ) + 32);
            if (limit <= 0)
                continue;

            _placeLegacyDoublePlant(blocks, plant, plantX, nextInt(random, limit), plantZ, random);
        }
    }
}

void OverworldRulePlacer::_placeLegacyDoublePlant(BlockManager &blocks, Block (*plant)(), int32_t x, int32_t y,
                                                  int32_t z, std::mt19937 &random) {
    // the game's double plant feature (sub_14D208760): only near the highest heightmap of the chunks around, then
    // 64 tries jittered by (g & 7) - (g & 7) on z and x and (g & 3) - (g & 3) on y, z drawn first
    int32_t highest = 0;
    for (int32_t chunkX = (x - 7) >> 4; chunkX <= (x + 7) >> 4; chunkX++) {
        for (int32_t chunkZ = (z - 7) >> 4; chunkZ <= (z + 7) >> 4; chunkZ++) {
            LevelChunk *chunk = blocks.getLevel().peekChunkPtr(chunkX, chunkZ);
            if (chunk == nullptr)
                continue;

            for (int32_t column = 0; column < 256; column++)
                highest = std::max(highest, chunk->getHeight(column & 15, column >> 4) + 1);
        }
    }
    if (y - 4 > highest)
        return;

    const BlockState state = plant().toBlockState();
    Tag lowerStates = state.mStates;
    lowerStates.putByte("upper_block_bit", 0);
    Tag upperStates = state.mStates;
    upperStates.putByte("upper_block_bit", 1);
    const BlockState lower(state.mName, lowerStates);
    const BlockState upper(state.mName, upperStates);

    for (int32_t attempt = 0; attempt < 64; attempt++) {
        const uint32_t firstZ = random() & 7u;
        const int32_t offsetZ = static_cast<int32_t>(firstZ) - static_cast<int32_t>(random() & 7u);
        const uint32_t firstY = random() & 3u;
        const int32_t offsetY = static_cast<int32_t>(firstY) - static_cast<int32_t>(random() & 3u);
        const uint32_t firstX = random() & 7u;
        const int32_t offsetX = static_cast<int32_t>(firstX) - static_cast<int32_t>(random() & 7u);
        const int32_t plantX = x + offsetX;
        const int32_t plantY = y + offsetY;
        const int32_t plantZ = z + offsetZ;
        // DoublePlantBlock::mayPlace: air with air above, on a block a plant grows on
        if (blocks.getBlockAt(plantX, plantY, plantZ).mName != "minecraft:air"
            || blocks.getBlockAt(plantX, plantY + 1, plantZ).mName != "minecraft:air"
            || !IFeature::isSupportDirt(blocks.getBlockAt(plantX, plantY - 1, plantZ)))
            continue;

        blocks.setBlockStateAt(plantX, plantY, plantZ, lower);
        blocks.setBlockStateAt(plantX, plantY + 1, plantZ, upper);
    }
}
