#include "World/Level/LevelGen/Overworld/Feature/OverworldRulePlacer.h"

#include "World/Level/LevelGen/Nether/Feature/NetherFeatures.h"
#include "World/Level/Block/Actor/BeehiveBlockActor.h"

#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/FenceBlock.h"
#include "World/Level/Block/FloweredAzaleaLeavesBlock.h"
#include "World/Level/Block/IceBlock.h"
#include "World/Level/Block/KelpBlock.h"
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
#include "World/Level/LevelGen/Biome/BiomeIds.h"
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
#include "World/Level/LevelGen/Overworld/Terrain/OverworldClimate.h"
#include "World/Level/LevelGen/Structure/StructureBlockTable.h"
#include "World/Level/LevelGen/Structure/StructureTemplate.h"

#include <algorithm>
#include <array>
#include <cctype>
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

    // sub_14A22D800: boost-style combine of the decoration seed with the rule's hash
    uint32_t ruleSeed(uint32_t decorationSeed, const std::string &identifier) {
        const uint32_t hash = static_cast<uint32_t>(hashedString(identifier));
        return decorationSeed ^ ((decorationSeed >> 2) + hash + (decorationSeed << 6) + 0x9E3779B9u);
    }

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
        return DecorationSupport::isAir(blocks.getBlockAt(x, y - 1, z))
               || DecorationSupport::isAir(blocks.getBlockAt(x, y + 1, z))
               || DecorationSupport::isAir(blocks.getBlockAt(x, y, z - 1))
               || DecorationSupport::isAir(blocks.getBlockAt(x, y, z + 1))
               || DecorationSupport::isAir(blocks.getBlockAt(x - 1, y, z))
               || DecorationSupport::isAir(blocks.getBlockAt(x + 1, y, z));
    }

    // LevelChunk::getAboveTopSolidBlock (sub_1438650A0) as the query calls it, water and leaves not counted: leaves
    // (material type 7) are looked through, and the first solid block below them ends the scan
    int32_t aboveTopSolid(BlockManager &blocks, int32_t x, int32_t z) {
        for (int32_t y = LevelChunk::MAX_Y; y >= LevelChunk::MIN_Y; y--) {
            const BlockState &state = blocks.getBlockAt(x, y, z);
            if (DecorationSupport::isAir(state))
                continue;
            if (VanillaBlocks::getAs<LeavesBlock>(state.mName) != nullptr
                || VanillaBlocks::getAs<FloweredAzaleaLeavesBlock>(state.mName) != nullptr)
                continue;

            if (BlockSupport::isSolid(state))
                return y + 1;
        }
        return LevelChunk::MIN_Y;
    }

    // what the game hands every rule and the features it places besides their position and random: the chunk's
    // decoration random of sub_14CDD4880 in the render params, which math.random draws its nextFloat from
    // (sub_1437FBE80, sub_140AE3310), and the generator's preliminary surface levels (sub_145C321F0)
    struct PlacementContext {
        std::mt19937 *mDecoration = nullptr;
        const OverworldClimate *mClimate = nullptr;
        // the second value of Random::nextGaussian, which the Random keeps for its next call
        const std::mt19937 *mGaussianOwner = nullptr;
        float mGaussian = 0.0f;
    };
    thread_local PlacementContext placementContext;

    using FeatureRandom::nextBoolean;
    using FeatureRandom::nextDouble;

    // Random::nextGaussian in floats (inlined in sub_145A3BFE0): the polar method on two raw draws, the second
    // value kept for the next call
    float nextGaussian(std::mt19937 &random) {
        if (placementContext.mGaussianOwner == &random) {
            placementContext.mGaussianOwner = nullptr;
            return placementContext.mGaussian;
        }

        float first = 0.0f;
        float second = 0.0f;
        float square = 0.0f;
        do {
            do {
                const uint32_t firstDraw = random();
                const uint32_t secondDraw = random();
                first = static_cast<float>(static_cast<double>(firstDraw) * 2.3283064365386963e-10);
                second = static_cast<float>(static_cast<double>(secondDraw) * 2.3283064365386963e-10);
                first = (first + first) + -1.0f;
                second = (second + second) + -1.0f;
                square = second * second + first * first;
            } while (square >= 1.0f);
        } while (square == 0.0f);

        const float scale = std::sqrt(std::log(square) * -2.0f / square);
        placementContext.mGaussianOwner = &random;
        placementContext.mGaussian = second * scale;
        return first * scale;
    }

    // the result of sub_14A290200: whether the start passed, then whether the walk ended on a surface and how many
    // blocks it passed before it
    struct ColumnEnd {
        bool mValid = false;
        bool mFound = false;
        int32_t mSteps = 0;
    };

    // sub_14A290200: from a start that passes, along the step while the blocks pass, at most range - 1 of them,
    // then the block it stopped at is tested as the surface
    template<typename Continues, typename Ends>
    ColumnEnd walkColumn(int32_t x, int32_t y, int32_t z, const int32_t step[3], int32_t range,
                         const Continues &continues, const Ends &ends) {
        ColumnEnd end;
        if (!continues(x, y, z))
            return end;

        end.mValid = true;
        int32_t blockX = x + step[0];
        int32_t blockY = y + step[1];
        int32_t blockZ = z + step[2];
        while (end.mSteps < range - 1 && continues(blockX, blockY, blockZ)) {
            blockX += step[0];
            blockY += step[1];
            blockZ += step[2];
            end.mSteps++;
        }
        end.mFound = ends(blockX, blockY, blockZ);
        return end;
    }

    // the six faces in the game's order: down, up, north, south, west, east
    const int32_t FACE_STEPS[6][3] = {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};

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

        float random() const override {
            return placementContext.mDecoration != nullptr ? nextFloat(*placementContext.mDecoration) : 0.0f;
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

    bool isKelp(const BlockState &state) {
        static const BlockStateSet KELP = BlockStateSet::ofBlocks([](const Block &block) {
            return dynamic_cast<const KelpBlock *>(&block) != nullptr;
        });
        return KELP.contains(state);
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
        static const BlockStateSet REFUSED = {
                "minecraft:air", "minecraft:bedrock", "minecraft:packed_ice", "minecraft:blue_ice"
        };
        return REFUSED.contains(state) || isWater(state)
               || LavaBlock::matches(state.mName) || IceBlock::matches(state.mName);
    }

    // the two (sub_140391CC0 & 7) differences of the kelp and seagrass jitter, z drawn first
    void jitter(std::mt19937 &random, int32_t &dx, int32_t &dz) {
        const uint32_t firstZ = random() & 7u;
        dz = static_cast<int32_t>(firstZ) - static_cast<int32_t>(random() & 7u);
        const uint32_t firstX = random() & 7u;
        dx = static_cast<int32_t>(firstX) - static_cast<int32_t>(random() & 7u);
    }

    bool isLava(BlockManager &blocks, int32_t x, int32_t y, int32_t z) {
        return LavaBlock::matches(blocks.getBlockAt(x, y, z).mName)
               || LavaBlock::matches(blocks.getExtraBlockAt(x, y, z).mName);
    }

    // DripstoneUtils' empty or water: air, or water on either layer
    bool isEmptyOrWater(BlockManager &blocks, int32_t x, int32_t y, int32_t z) {
        const BlockState &state = blocks.getBlockAt(x, y, z);
        return DecorationSupport::isAir(state) || isWater(state) || isWater(blocks.getExtraBlockAt(x, y, z));
    }

    // the blocks dripstone replaces (xmmword_1529AA840)
    bool isDripstoneReplaceable(const BlockState &state) {
        static const BlockStateSet REPLACEABLE = {
                "minecraft:dirt", "minecraft:coarse_dirt", "minecraft:stone", "minecraft:tuff", "minecraft:deepslate"
        };
        return REPLACEABLE.contains(state);
    }

    bool isDripstoneBlock(const BlockState &state) {
        static const BlockStateSet DRIPSTONE = {VanillaBlocks::DRIPSTONE_BLOCK().getIdentifier()};
        return DRIPSTONE.contains(state);
    }

    bool isPointedDripstone(const BlockState &state) {
        static const BlockStateSet POINTED = {VanillaBlocks::POINTED_DRIPSTONE().getIdentifier()};
        return POINTED.contains(state);
    }

    // sub_14721CC10
    bool isDripstoneBaseOrLava(BlockManager &blocks, int32_t x, int32_t y, int32_t z) {
        const BlockState &state = blocks.getBlockAt(x, y, z);
        return isDripstoneReplaceable(state) || isDripstoneBlock(state) || isLava(blocks, x, y, z);
    }

    // sub_1471E6440
    bool placeDripstoneBlock(BlockManager &blocks, int32_t x, int32_t y, int32_t z) {
        if (!isDripstoneReplaceable(blocks.getBlockAt(x, y, z)))
            return false;

        static const BlockState dripstone = VanillaBlocks::DRIPSTONE_BLOCK().toBlockState();
        blocks.setBlockStateAt(x, y, z, dripstone);
        return true;
    }

    // sub_1471E6570: from its base, base, middle, frustum and the tip or merge, as long as the blocks are open
    void growPointedDripstone(BlockManager &blocks, int32_t x, int32_t y, int32_t z, bool up, int32_t length,
                              bool merge) {
        if (length <= 0)
            return;

        // sub_14721CE00: thickness 0 tip, 1 frustum, 2 middle, 3 base, 4 merge
        static const char *const THICKNESS[5] = {"tip", "frustum", "middle", "base", "merge"};
        static const BlockState pointed = VanillaBlocks::POINTED_DRIPSTONE().toBlockState();
        const auto place = [&](int32_t blockY, int32_t thickness) {
            BlockState state = withState(pointed, "dripstone_thickness", THICKNESS[thickness]);
            Tag states = state.mStates;
            states.putByte("hanging", up ? 0 : 1);
            setInWater(blocks, x, blockY, z, BlockState(state.mName, states));
        };
        const auto open = [&](int32_t blockY) {
            return isEmptyOrWater(blocks, x, blockY, z) && !isLava(blocks, x, blockY, z);
        };

        if (!open(y))
            return;

        const int32_t step = up ? 1 : -1;
        place(y, length == 2 ? 1 : (length == 1 ? (merge ? 4 : 0) : 3));
        int32_t blockY = y + step;
        for (int32_t remaining = length; remaining > 1 && open(blockY); remaining--) {
            place(blockY, remaining == 3 ? 1 : (remaining == 2 ? (merge ? 4 : 0) : 2));
            blockY += step;
            if (remaining <= 2)
                break;
        }
    }

    // LargeDripstone's height at a distance from its axis (sub_14721CD00), in doubles rounded to a float
    float largeDripstoneHeight(float distance, float radius, float scale, float bluntness) {
        const double ratio = static_cast<double>(std::fmax(bluntness, distance) / radius) * 0.384;
        const double height = std::pow(ratio, 1.3333333333333333) * 0.75 - std::pow(ratio, 0.6666666666666666)
                              + std::log(ratio) * -0.3333333333333333;
        return static_cast<float>(static_cast<double>(radius)
                                  * (std::fmax(height * static_cast<double>(scale), 0.0) / 0.384));
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
    // survives on nylium and soul soil besides the blocks of PlantSupport::Dirt
    bool mNetherPlant = false;
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
    const OverworldTreePlacer::Mega *mMega = nullptr;
};

// one of the game's legacy tree features (LegacyTreeFeature, constructor sub_14D21F620, place sub_14D224440): a
// count of trees from the density, each feature picked by the type's own draws
struct OverworldRulePlacer::LegacyTreeDefinition {
    enum class Pick {
        // nextInt(5) != 0 ? (nextInt(10) != 0 ? third : second) : first
        Forest,
        First,
        // nextInt(3) == 0 ? first : second
        OneInThree,
        // nextInt(3) != 0 ? (nextInt(3) != 0 ? fourth : third) : (nextInt(13) != 0 ? second : first), sub_14D230860
        MegaTaiga,
        // nextInt(20) != 0 ? second : first, sub_14D230100
        Swamp,
        // nextInt(5) != 0 && nextBoolean() ? second : first, sub_14D2302A0
        BirchMutated
    };

    const char *mIdentifier;
    float mDensity;
    Pick mPick;
    std::vector<Placed> mFeatures;
    // the swamp's ten seagrass_feature tries at columns of the chunk, 54 above the rule's y, before the trees
    const SeagrassDefinition *mSeagrass = nullptr;
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

// a minecraft:surface_relative_threshold_feature (place sub_14A280DE0)
struct OverworldRulePlacer::ThresholdDefinition {
    const char *mIdentifier;
    Placed mPlaces;
    int32_t mMinimumDistanceBelowSurface;
};

// a minecraft:snap_to_surface_feature (place sub_14A2714C0); surface 0 is ceiling, 1 floor, 2 wall and 3
// random_horizontal
struct OverworldRulePlacer::SnapDefinition {
    const char *mIdentifier;
    Placed mPlaces;
    int32_t mVerticalSearchRange;
    int32_t mSurface;
    bool mAllowAirPlacement;
    bool mAllowNonAirPlacement;
    bool mAllowUnderwaterPlacement;
    bool mEmbedInSurface;
    std::vector<std::string> mAllowedSurfaceBlocks;
};

// a minecraft:partially_exposed_blob_feature (place sub_14D22BD50); faces 0 down to 5 east
struct OverworldRulePlacer::BlobDefinition {
    const char *mIdentifier;
    Block (*mPlaces)();
    int32_t mPlacementRadiusAroundFloor;
    float mPlacementProbabilityPerValidPosition;
    int32_t mExposedFace;
};

// a minecraft:multiface_feature (place sub_14A2CD220); faces 0 down to 5 east
struct OverworldRulePlacer::MultifaceDefinition {
    const char *mIdentifier;
    Block (*mPlaces)();
    int32_t mSearchRange;
    std::vector<int32_t> mFaces;
    float mChanceOfSpreading;
    std::vector<std::string> mCanPlaceOn;
};

// one of the game's legacy flower features (LegacyFlowerFeature, vtable 0x14F4A1490, place sub_14D21CB80); type 1 is
// the forest, 2 the overworld and 3 the plains
struct OverworldRulePlacer::LegacyFlowerDefinition {
    const char *mIdentifier;
    int32_t mType;
};

// a minecraft:fossil_feature
struct OverworldRulePlacer::FossilDefinition {
    const char *mIdentifier;
    const char *mOre;
    int32_t mMaxEmptyCorners;
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
            {"minecraft:tuff_feature", 64, 0.0f, {{&VanillaBlocks::TUFF, {"minecraft:stone", "minecraft:deepslate"}}}},
            {"minecraft:nether_soul_sand_underground_feature", 12, 0.0f,
             {{&VanillaBlocks::SOUL_SAND, {"minecraft:netherrack"}}}}
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
            {"minecraft:grass_double_plant_upper_feature", &VanillaBlocks::TALL_GRASS, false, false, 1},
            {"minecraft:bush_feature", &VanillaBlocks::BUSH},
            {"minecraft:crimson_fungus_feature", &VanillaBlocks::CRIMSON_FUNGUS, true, false, -1, false, true},
            {"minecraft:warped_fungus_feature", &VanillaBlocks::WARPED_FUNGUS, true, false, -1, false, true},
            {"minecraft:crimson_roots_feature", &VanillaBlocks::CRIMSON_ROOTS, true, false, -1, false, true},
            {"minecraft:warped_roots_feature", &VanillaBlocks::WARPED_ROOTS, true, false, -1, false, true},
            {"minecraft:nether_sprouts_feature", &VanillaBlocks::NETHER_SPROUTS, true, false, -1, false, true}
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
            {"minecraft:crimson_fungus_scatter_feature", "24", {{2, "-8", "8"}, {2, "-4", "4"}, {2, "-8", "8"}}, 5, single(7)},
            {"minecraft:warped_fungus_scatter_feature", "24", {{2, "-8", "8"}, {2, "-4", "4"}, {2, "-8", "8"}}, 5, single(8)},
            {"minecraft:crimson_roots_scatter_feature", "64", {{2, "-8", "8"}, {2, "-4", "4"}, {2, "-8", "8"}}, 5, single(9)},
            {"minecraft:warped_roots_scatter_feature", "64", {{2, "-8", "8"}, {2, "-4", "4"}, {2, "-8", "8"}}, 5, single(10)},
            {"minecraft:nether_sprouts_scatter_feature", "64", {{2, "-8", "8"}, {2, "-4", "4"}, {2, "-8", "8"}}, 5, single(11)},
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
            {"minecraft:scatter_taiga_plant_feature", "32", {GRASS_SIDE, GRASS_HEIGHT, GRASS_SIDE}, 5, weighted(0)},
            {"minecraft:scatter_bush_feature", "24", {{6, "-5", "5"}, {6, "-3", "3"}, {6, "-5", "5"}}, 5, single(6)}
    };

    // registered in sub_14726F340
    static const LegacyFlowerDefinition LEGACY_FLOWERS[] = {
            {"minecraft:legacy:overworld_flower_feature", 2},
            {"minecraft:legacy:plains_flower_feature", 3}
    };
    static const auto legacyFlower = [](size_t index) {
        Placed placed;
        placed.mLegacyFlower = &LEGACY_FLOWERS[index];
        return placed;
    };
    // BDS 1.26.61 behavior packs, features/scatter_overworld_flower_feature.json and scatter_plains_flower_feature.json
    static const ScatterDefinition FLOWER_SCATTERS[] = {
            {"minecraft:scatter_overworld_flower_feature", "64", {{6, "-7", "7"}, {6, "-3", "3"}, {6, "-7", "7"}}, 5,
             legacyFlower(0)},
            {"minecraft:scatter_plains_flower_feature", "64", {{6, "-6", "6"}, {6, "-2", "2"}, {6, "-6", "6"}}, 5,
             legacyFlower(1)}
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
    static const auto blob = [](const BlobDefinition &definition) {
        Placed placed;
        placed.mBlob = &definition;
        return placed;
    };
    static const auto multiface = [](const MultifaceDefinition &definition) {
        Placed placed;
        placed.mMultiface = &definition;
        return placed;
    };
    static const auto snap = [](const SnapDefinition &definition) {
        Placed placed;
        placed.mSnap = &definition;
        return placed;
    };
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

    // BDS 1.26.61 behavior packs, features/swamp_tree_feature.json
    static const TreeDefinition SWAMP_TREE = {
            {"minecraft:swamp_tree_feature", Tree::Trunk::Simple, &VanillaBlocks::OAK_LOG, {5, 9}, {0, 0},
             Tree::Canopy::Simple, &VanillaBlocks::OAK_LEAVES, {0, 0}, {0, 0}, {0, 0}, 0, 0.0f, false, 2, 2, 1, 4, 4,
             true}, nullptr, {}};
    static const Placed HUGE_MUSHROOM = [] {
        Placed placed;
        placed.mHugeMushroom = true;
        return placed;
    }();

    // BDS 1.26.61 behavior packs, features/mega_spruce_tree_feature.json and mega_pine_tree_feature.json
    static const Tree::Mega MEGA_SPRUCE = {2, 13, {3, 15}, {13, 18}, 0, 3.5f, 2, &VanillaBlocks::PODZOL, 5, 2};
    static const Tree::Mega MEGA_PINE = {2, 13, {3, 15}, {3, 8}, 0, 3.5f, 2, &VanillaBlocks::PODZOL, 5, 2};
    static const TreeDefinition MEGA_TREES[] = {
            {{"minecraft:mega_spruce_tree_feature", Tree::Trunk::Mega, &VanillaBlocks::SPRUCE_LOG, {0, 0}, {0, 0},
              Tree::Canopy::MegaPine, &VanillaBlocks::SPRUCE_LEAVES, {0, 0}, {0, 0}, {0, 0}, 0, 0.0f, false}, nullptr,
             {}, &MEGA_SPRUCE},
            {{"minecraft:mega_pine_tree_feature", Tree::Trunk::Mega, &VanillaBlocks::SPRUCE_LOG, {0, 0}, {0, 0},
              Tree::Canopy::MegaPine, &VanillaBlocks::SPRUCE_LEAVES, {0, 0}, {0, 0}, {0, 0}, 0, 0.0f, false}, nullptr,
             {}, &MEGA_PINE}
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
            // types 1 and 2 of the constructor; the mutated forest's second feature, select_super_birch_tree_feature,
            // is in none of the game's packs and places nothing
            {"minecraft:legacy:birch_forest_tree_feature", 10.0f, Pick::First, {aggregate(SELECT_BIRCH)}},
            {"minecraft:legacy:birch_forest_mutated_tree_feature", 10.0f, Pick::BirchMutated, {aggregate(SELECT_BIRCH)}},
            {"minecraft:legacy:plains_tree_feature", 0.05f, Pick::OneInThree,
             {aggregate(FANCY_WITH_BEEHIVE), aggregate(SELECT_OAK)}},
            {"minecraft:legacy:taiga_tree_feature", 10.0f, Pick::OneInThree,
             {tree(TREES[9]), aggregate(SELECT_SPRUCE)}},
            {"minecraft:legacy:mega_taiga_tree_feature", 10.0f, Pick::MegaTaiga,
             {tree(MEGA_TREES[0]), tree(MEGA_TREES[1]), tree(TREES[9]), aggregate(SELECT_SPRUCE)}},
            // the swamp foliage feature (constructor sub_14D21E9F0, place sub_14D21F070) counts its trees like a
            // legacy tree feature of density 2
            {"minecraft:legacy:swamp_foliage_feature", 2.0f, Pick::Swamp, {HUGE_MUSHROOM, tree(SWAMP_TREE)},
             &SEAGRASS[3]}
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

    // BDS 1.26.61 behavior packs, features/underwater_magma_*.json
    static const BlobDefinition UNDERWATER_MAGMA = {
            "minecraft:underwater_magma_feature", &VanillaBlocks::MAGMA, 1, 0.5f, 1};
    static const SnapDefinition UNDERWATER_MAGMA_SNAP = {
            "minecraft:underwater_magma_snap_to_surface_feature", blob(UNDERWATER_MAGMA), 5, 1, false, false, true,
            false, {}};
    static const ThresholdDefinition UNDERWATER_MAGMA_UNDERGROUND = {
            "minecraft:underwater_magma_underground_feature", snap(UNDERWATER_MAGMA_SNAP), 2};

    // BDS 1.26.61 behavior packs, features/*glow_lichen_feature.json: the ceiling and the four walls
    static const MultifaceDefinition GLOW_LICHEN = {
            "minecraft:glow_lichen_feature", &VanillaBlocks::GLOW_LICHEN, 20, {1, 2, 3, 4, 5}, 0.5f,
            {"minecraft:stone", "minecraft:andesite", "minecraft:diorite", "minecraft:granite",
             "minecraft:dripstone_block", "minecraft:calcite", "minecraft:tuff", "minecraft:deepslate",
             "minecraft:sulfur", "minecraft:cinnabar"}};
    static const ThresholdDefinition UNDERGROUND_GLOW_LICHEN = {
            "minecraft:underground_glow_lichen_feature", multiface(GLOW_LICHEN), 2};

    // BDS 1.26.61 behavior packs, features/small_dripstone*.json; the scatter keeps the default xyz order
    static const Placed POINTED_DRIPSTONE = [] {
        Placed placed;
        placed.mPointedDripstone = true;
        return placed;
    }();
    static const SnapDefinition SMALL_DRIPSTONE_SNAP = {
            "minecraft:small_dripstone_snap_to_surface_feature", POINTED_DRIPSTONE, 12, 3, true, false, true, false,
            {}};
    static const ScatterDefinition SMALL_DRIPSTONE = {
            "minecraft:small_dripstone_feature", "Math.Random(1, 5)",
            {{2, "-10", "10"}, {2, "-2", "2"}, {2, "-10", "10"}}, 0, snap(SMALL_DRIPSTONE_SNAP)};

    // BDS 1.26.61 behavior packs, features/amethyst_geode_feature.json
    static const GeodeDefinition GEODES[] = {
            {"minecraft:amethyst_geode_feature", &VanillaBlocks::AIR, &VanillaBlocks::AMETHYST_BLOCK,
             &VanillaBlocks::BUDDING_AMETHYST, &VanillaBlocks::CALCITE, &VanillaBlocks::SMOOTH_BASALT,
             {&VanillaBlocks::AMETHYST_CLUSTER, &VanillaBlocks::LARGE_AMETHYST_BUD, &VanillaBlocks::MEDIUM_AMETHYST_BUD,
              &VanillaBlocks::SMALL_AMETHYST_BUD},
             4, 7, 3, 5, 1, 3, 16, 2, 0.025f, 0.35f, 0.083f, true, 1}
    };

    struct NetherPlaced {
        const char *mIdentifier;
        bool (*mFunction)(BlockManager &, int32_t, int32_t, int32_t, std::mt19937 &);
    };
    static const NetherPlaced NETHER_FEATURES[] = {
            {"minecraft:_huge_crimson_fungus_feature", &NetherFeatures::placeHugeCrimsonFungus},
            {"minecraft:_huge_warped_fungus_feature", &NetherFeatures::placeHugeWarpedFungus},
            {"minecraft:_weeping_vines_cluster_feature", &NetherFeatures::placeWeepingVines},
            {"minecraft:_twisting_vines_cluster_feature", &NetherFeatures::placeTwistingVines},
            {"minecraft:_basalt_pillar_feature", &NetherFeatures::placeBasaltPillar},
            {"minecraft:delta_feature", &NetherFeatures::placeDelta},
            {"minecraft:basalt_columns_feature", &NetherFeatures::placeBasaltColumns}
    };

    // BDS 1.26.61 behavior packs, features/*fossil_feature.json
    static const std::vector<FossilDefinition> FOSSILS = {
            {"minecraft:fossil_feature", "minecraft:coal_ore", 4},
            {"minecraft:deepslate_diamond_fossil_feature", "minecraft:deepslate_diamond_ore", 4}
    };

    Feature feature;
    if (entry.mPass == AFTER_SURFACE_PASS) {
        // vtable 0x14F4A0F60, place sub_14D217680
        feature.mKelp = entry.mFeature == "minecraft:kelp_feature";
        if (entry.mFeature == UNDERWATER_MAGMA_UNDERGROUND.mIdentifier)
            feature.mPlaced.mThreshold = &UNDERWATER_MAGMA_UNDERGROUND;
        if (entry.mFeature == SMALL_DRIPSTONE.mIdentifier)
            feature.mPlaced.mScatter = &SMALL_DRIPSTONE;
        feature.mPlaced.mDripstoneCluster = entry.mFeature == "minecraft:dripstone_cluster_feature";
        feature.mLegacyIce = entry.mFeature == "minecraft:legacy:ice_feature";
        for (const FossilDefinition &fossil: FOSSILS) {
            if (entry.mFeature == fossil.mIdentifier)
                feature.mFossil = &fossil;
        }
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
        feature.mPlaced.mLargeDripstone = entry.mFeature == "minecraft:large_dripstone_feature";
        for (const NetherPlaced &nether: NETHER_FEATURES) {
            if (entry.mFeature == nether.mIdentifier)
                feature.mPlaced.mFunction = nether.mFunction;
        }
        feature.mSprings = entry.mFeature == "minecraft:legacy:springs_feature";
        for (const LegacyTreeDefinition &trees: LEGACY_TREES) {
            if (entry.mFeature == trees.mIdentifier)
                feature.mLegacyTree = &trees;
        }
    }
    if (entry.mPass == "after_underground_pass" && entry.mFeature == UNDERGROUND_GLOW_LICHEN.mIdentifier)
        feature.mPlaced.mThreshold = &UNDERGROUND_GLOW_LICHEN;
    if (entry.mPass == FIRST_PASS) {
        feature.mForestFoliage = entry.mFeature == "minecraft:legacy:forest_foliage_feature";
        feature.mForestRock = entry.mFeature == "minecraft:legacy:forest_rock_feature";
    }
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
        for (const ScatterDefinition &scatter: FLOWER_SCATTERS) {
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

void OverworldRulePlacer::apply(BlockManager &blocks, const LevelChunk &chunk, const OverworldClimate &climate,
                               uint32_t levelSeed, const std::vector<int32_t> &biomes) {
    _apply(blocks, chunk, &climate, levelSeed, biomes);
}

void OverworldRulePlacer::applyBiome(BlockManager &blocks, const LevelChunk &chunk, uint32_t levelSeed,
                                     int32_t biome) {
    _apply(blocks, chunk, nullptr, levelSeed, {biome});
}

void OverworldRulePlacer::_apply(BlockManager &blocks, const LevelChunk &chunk, const OverworldClimate *climate,
                                 uint32_t levelSeed, const std::vector<int32_t> &biomes) {
    // the decoration seed of sub_14CDD4880, from the level seed and the chunk position
    std::mt19937 random(levelSeed);
    const uint32_t multiplierX = (random() >> 1) | 1u;
    const uint32_t multiplierZ = (random() >> 1) | 1u;
    const uint32_t decorationSeed = (multiplierZ * static_cast<uint32_t>(chunk.getZ())
                                     + multiplierX * static_cast<uint32_t>(chunk.getX())) ^ levelSeed;

    std::mt19937 decoration(decorationSeed);
    placementContext.mDecoration = &decoration;
    placementContext.mClimate = climate;

    // sub_14CDDF960, sub_14A22E1E0, sub_14A22E470: each pass, every biome of the chunk, then its rules in order
    for (const char *pass: PASSES) {
        for (int32_t biome: biomes) {
            const std::vector<BiomeFeatureEntry> *rules = BiomeChunkGenDataRegistry::getConsolidatedFeatures(biome);
            if (rules == nullptr)
                continue;

            for (const BiomeFeatureEntry &entry: *rules) {
                if (entry.mPass != pass)
                    continue;

                if (handles(entry))
                    _placeRule(blocks, chunk, entry, _find(entry), decorationSeed, biomes.size() > 1 ? biome : -1);
                else
                    _drawUnported(blocks, chunk, entry, decorationSeed);
            }
        }
    }
    placementContext = PlacementContext();
}

void OverworldRulePlacer::drawBiomeRules(BlockManager &blocks, const LevelChunk &chunk, int32_t biome,
                                         uint32_t decorationSeed, std::mt19937 &shared) {
    const std::vector<BiomeFeatureEntry> *rules = BiomeChunkGenDataRegistry::getConsolidatedFeatures(biome);
    if (rules == nullptr)
        return;

    placementContext.mDecoration = &shared;
    const int32_t origin[3] = {chunk.getX() << 4, 0, chunk.getZ() << 4};
    for (const char *pass: PASSES) {
        for (const BiomeFeatureEntry &entry: *rules) {
            if (entry.mPass != pass)
                continue;

            shared.seed(ruleSeed(decorationSeed, entry.mIdentifier));
            ScatterVariables variables(blocks, origin);
            const ExpressionPtr iterationsExpression = FeatureExpression::parse(entry.mIterations);
            const ExpressionPtr percent = FeatureExpression::parse(entry.mChancePercent);
            if (iterationsExpression == nullptr || entry.mEvalOrder < 0 || entry.mEvalOrder > 5)
                continue;

            if (entry.mChanceNumerator > 0 && entry.mChanceDenominator > 0) {
                if (entry.mChanceNumerator != entry.mChanceDenominator
                    && nextInt(shared, entry.mChanceDenominator) >= entry.mChanceNumerator)
                    continue;
            } else {
                const float chance = percent == nullptr ? 0.0f : percent->evaluate(variables);
                if (chance < 100.0f && (chance <= 0.0f || chance <= nextFloat(shared) * 100.0f))
                    continue;
            }

            // measured on the end islands: the iteration count is truncated there, not rounded
            const float count = iterationsExpression->evaluate(variables);
            int32_t iterations = count > 0.0f ? static_cast<int32_t>(count) : 0;

            Axis axes[3];
            bool parsed = true;
            for (size_t axis = 0; axis < 3; axis++) {
                const BiomeFeatureEntry::Coordinate &coordinate = entry.mCoordinates[axis];
                parsed = parseAxis(coordinate.mDistribution, coordinate.mMin, coordinate.mMax, axes[axis]) && parsed;
            }
            if (!parsed)
                continue;

            while (iterations-- > 0) {
                int32_t position[3] = {0, 0, 0};
                scatterPosition(axes, entry.mEvalOrder, origin, variables, shared, position);
            }
        }
    }
    placementContext = PlacementContext();
}

void OverworldRulePlacer::_drawUnported(BlockManager &blocks, const LevelChunk &chunk, const BiomeFeatureEntry &entry,
                                        uint32_t decorationSeed) {
    // a rule the placer does not port still draws its math.random from the decoration random the ported rules
    // after it share, for its chance, its iterations and, when they draw, its coordinates
    // runs for every unported rule of every chunk: searched in place, without a lowercased copy
    const auto draws = [](const std::string &text) {
        static const std::string RANDOM = "math.random";
        const auto sameLetter = [](char left, char right) {
            return std::tolower(static_cast<unsigned char>(left)) == right;
        };
        return std::search(text.begin(), text.end(), RANDOM.begin(), RANDOM.end(), sameLetter) != text.end();
    };

    bool coordinatesDraw = false;
    for (const BiomeFeatureEntry::Coordinate &coordinate: entry.mCoordinates)
        coordinatesDraw = coordinatesDraw || draws(coordinate.mMin) || draws(coordinate.mMax);
    const bool percentDraws = (entry.mChanceNumerator <= 0 || entry.mChanceDenominator <= 0)
                              && draws(entry.mChancePercent);
    if (!coordinatesDraw && !percentDraws && !draws(entry.mIterations))
        return;

    const ExpressionPtr iterationsExpression = FeatureExpression::parse(entry.mIterations);
    const ExpressionPtr percent = FeatureExpression::parse(entry.mChancePercent);
    if (iterationsExpression == nullptr
        || ((entry.mChanceNumerator <= 0 || entry.mChanceDenominator <= 0) && percent == nullptr)
        || entry.mEvalOrder < 0 || entry.mEvalOrder > 5)
        return;

    Axis axes[3];
    for (size_t axis = 0; axis < 3; axis++) {
        const BiomeFeatureEntry::Coordinate &coordinate = entry.mCoordinates[axis];
        if (!parseAxis(coordinate.mDistribution, coordinate.mMin, coordinate.mMax, axes[axis]) && coordinatesDraw)
            return;
    }

    const uint32_t seed = ruleSeed(decorationSeed, entry.mIdentifier);
    std::mt19937 scatter(seed);
    const int32_t origin[3] = {chunk.getX() << 4, 0, chunk.getZ() << 4};
    ScatterVariables variables(blocks, origin);
    int32_t iterations = iterationsOf(entry.mChanceNumerator, entry.mChanceDenominator, percent,
                                      iterationsExpression, variables, scatter);
    if (!coordinatesDraw)
        return;

    while (iterations-- > 0) {
        int32_t position[3] = {0, 0, 0};
        scatterPosition(axes, entry.mEvalOrder, origin, variables, scatter, position);
    }
}

void OverworldRulePlacer::_placeRule(BlockManager &blocks, const LevelChunk &chunk, const BiomeFeatureEntry &entry,
                                     const Feature &feature, uint32_t decorationSeed, int32_t biomeFilter) {
    const uint32_t seed = ruleSeed(decorationSeed, entry.mIdentifier);
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
    placementContext.mGaussianOwner = nullptr;
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
        else if (feature.mForestRock)
            _placeForestRocks(blocks, position[0], position[2], placement);
        else if (feature.mSprings)
            _placeSprings(blocks, position[0], position[2], placement);
        else if (feature.mLegacyIce)
            _placeLegacyIce(blocks, position[0], position[2], placement);
        else if (feature.mFossil != nullptr)
            _placeFossil(blocks, *feature.mFossil, position[0], position[1], position[2], placement, seed);
        else if (feature.mLegacyTree != nullptr)
            _placeLegacyTrees(blocks, *feature.mLegacyTree, position[0], position[1], position[2], placement);
        else if (feature.mPlaced.isSet())
            _place(blocks, feature.mPlaced, position[0], position[1], position[2], placement);
        else
            _placeKelp(blocks, position[0], position[1], position[2], placement);
    }
}

bool OverworldRulePlacer::placeOreVein(BlockManager &blocks, int32_t count, Block (*places)(),
                                       const std::vector<std::string> &mayReplace, int32_t x, int32_t y, int32_t z,
                                       std::mt19937 &random) {
    const OreDefinition ore{"", count, 0.0f, {{places, mayReplace}}};
    return _placeOre(blocks, ore, x, y, z, random);
}

bool OverworldRulePlacer::_placeFossil(BlockManager &blocks, const FossilDefinition &fossil, int32_t x, int32_t y,
                                       int32_t z, std::mt19937 &random, uint32_t integritySeed) {
    // FossilFeature::place (sub_14D209B60); the game first gives up inside the bounds of a structure, which the
    // generator does not track yet
    static const char *const TEMPLATES[] = {
            "fossils/fossil_spine_01.nbt", "fossils/fossil_spine_02.nbt", "fossils/fossil_spine_03.nbt",
            "fossils/fossil_spine_04.nbt", "fossils/fossil_skull_01.nbt", "fossils/fossil_skull_02.nbt",
            "fossils/fossil_skull_03.nbt", "fossils/fossil_skull_04.nbt"};
    constexpr float BONE_INTEGRITY = 0.9f;
    constexpr float ORE_INTEGRITY = 0.1f;

    const int32_t rotation = nextInt(random, 4);
    const std::shared_ptr<const StructureTemplate> piece = StructureTemplate::load(TEMPLATES[nextInt(random, 8)]);
    if (piece == nullptr)
        return false;

    const bool turned = (rotation & 1) != 0;
    const int32_t spanX = turned ? piece->getSizeZ() : piece->getSizeX();
    const int32_t spanZ = turned ? piece->getSizeX() : piece->getSizeZ();
    const int32_t offsetX = nextInt(random, 16 - spanX);
    const int32_t offsetZ = nextInt(random, 16 - spanZ);

    int32_t lowest = y;
    for (int32_t i = 0; i < spanX; i++) {
        for (int32_t j = 0; j < spanZ; j++)
            lowest = std::min(lowest, aboveTopSolid(blocks, x + offsetX + i, z + offsetZ + j));
    }

    const int32_t startX = x + offsetX;
    const int32_t startY = std::max(lowest - nextInt(random, 10) - 15, LevelChunk::MIN_Y + 10);
    const int32_t startZ = z + offsetZ;

    // the pieces turn around their first block, so the far corner depends on the rotation
    const int32_t lastX = piece->getSizeX() - 1;
    const int32_t lastZ = piece->getSizeZ() - 1;
    const int32_t farX[] = {startX + lastX, startX - lastZ, startX - lastX, startX + lastZ};
    const int32_t farZ[] = {startZ + lastZ, startZ + lastX, startZ - lastZ, startZ - lastX};
    const int32_t topY = startY + piece->getSizeY() - 1;

    int32_t emptyCorners = 0;
    for (int32_t cornerX: {startX, farX[rotation]}) {
        for (int32_t cornerY: {topY, startY}) {
            for (int32_t cornerZ: {startZ, farZ[rotation]}) {
                const BlockState &corner = blocks.getBlockAt(cornerX, cornerY, cornerZ);
                if (DecorationSupport::isAir(corner) || isWater(corner) || LavaBlock::matches(corner.mName))
                    emptyCorners++;
            }
        }
    }
    if (emptyCorners > fossil.mMaxEmptyCorners)
        return false;

    // the game places the piece twice with one integrity seed: the second pass draws the same numbers, so it only
    // turns a tenth of the blocks of the first into ore, and its match at chance 1 draws nothing
    std::mt19937 integrity(integritySeed);
    const BlockState ore(fossil.mOre);
    for (const StructureTemplate::BlockEntry &entry: piece->getBlocks()) {
        BlockState bone;
        if (!StructureBlockTable::translate(piece->getPalette()[static_cast<size_t>(entry.mPaletteIndex)], rotation,
                                            bone) || DecorationSupport::isAir(bone))
            continue;

        const float draw = nextFloat(integrity);
        if (draw > BONE_INTEGRITY)
            continue;

        const int32_t turnedX[] = {entry.x, -entry.z, -entry.x, entry.z};
        const int32_t turnedZ[] = {entry.z, entry.x, -entry.z, -entry.x};
        blocks.setBlockStateAt(startX + turnedX[rotation], startY + entry.y, startZ + turnedZ[rotation],
                               draw <= ORE_INTEGRITY ? ore : bone);
    }
    return true;
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
    if (placed.mFunction != nullptr)
        return placed.mFunction(blocks, x, y, z, random);
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
    if (placed.mThreshold != nullptr)
        return _placeThreshold(blocks, *placed.mThreshold, x, y, z, random);
    if (placed.mSnap != nullptr)
        return _placeSnap(blocks, *placed.mSnap, x, y, z, random);
    if (placed.mBlob != nullptr)
        return _placeBlob(blocks, *placed.mBlob, x, y, z, random);
    if (placed.mMultiface != nullptr)
        return _placeMultiface(blocks, *placed.mMultiface, x, y, z, random);
    if (placed.mDripstoneCluster)
        return _placeDripstoneCluster(blocks, x, y, z, random);
    if (placed.mPointedDripstone)
        return _placePointedDripstone(blocks, x, y, z, random);
    if (placed.mLargeDripstone)
        return _placeLargeDripstone(blocks, x, y, z, random);
    if (placed.mLegacyFlower != nullptr)
        return _placeLegacyFlower(blocks, *placed.mLegacyFlower, x, y, z, random);
    if (placed.mHugeMushroom)
        return _placeHugeMushroom(blocks, x, y, z, random);
    return _placeBeehiveSearch(blocks, x, y, z, random);
}

bool OverworldRulePlacer::_placeHugeMushroom(BlockManager &blocks, int32_t x, int32_t y, int32_t z,
                                             std::mt19937 &random) {
    // HugeMushroomFeature (vtable 0x14F3D7A30, sub_14A2C8900) of the random type: brown or red, 4 to 6 tall and
    // twice as tall one time in 12, all drawn before any test
    const bool red = nextInt(random, 2) > 0;
    int32_t height = nextInt(random, 3) + 4;
    if (nextInt(random, 12) == 0)
        height *= 2;
    if (y <= LevelChunk::MIN_Y || y + height + 1 >= LevelChunk::MAX_Y + 1)
        return false;

    // air or leaves (material types 0 and 7) over the stem and, from 4 above the ground, the cap's 7 by 7; a small
    // mushroom may stand at the position itself
    static const BlockStateSet SMALL = {
            VanillaBlocks::BROWN_MUSHROOM().getIdentifier(), VanillaBlocks::RED_MUSHROOM().getIdentifier()
    };
    for (int32_t at = y; at <= y + height + 1; at++) {
        const int32_t radius = at > y + 3 ? 3 : 0;
        for (int32_t offsetX = -radius; offsetX <= radius; offsetX++) {
            for (int32_t offsetZ = -radius; offsetZ <= radius; offsetZ++) {
                if (at < LevelChunk::MIN_Y || at > LevelChunk::MAX_Y)
                    return false;
                const BlockState &state = blocks.getBlockAt(x + offsetX, at, z + offsetZ);
                if (DecorationSupport::isAir(state) || VanillaBlocks::getAs<LeavesBlock>(state.mName) != nullptr)
                    continue;
                if (offsetX == 0 && at == y && offsetZ == 0 && SMALL.contains(state))
                    continue;
                return false;
            }
        }
    }

    // the lists at xmmword_1529B3B88 and xmmword_1529B0A98 (sub_14A2C96E0), grass and dirt with roots; the game also
    // grows it from water in some biomes, a case not ported
    static const BlockStateSet GROUND = {
            VanillaBlocks::DIRT().getIdentifier(), VanillaBlocks::COARSE_DIRT().getIdentifier(),
            VanillaBlocks::MYCELIUM().getIdentifier(), VanillaBlocks::CRIMSON_NYLIUM().getIdentifier(),
            VanillaBlocks::WARPED_NYLIUM().getIdentifier(), VanillaBlocks::PODZOL().getIdentifier(),
            VanillaBlocks::GRASS_BLOCK().getIdentifier(), VanillaBlocks::DIRT_WITH_ROOTS().getIdentifier()
    };
    if (!GROUND.contains(blocks.getBlockAt(x, y - 1, z)))
        return false;

    // the cap: a red one hangs three layers of radius 2 below a top of radius 1, a brown one is a single layer of
    // radius 3; huge_mushroom_bits 1 to 9 face the cell's side of the square, 5 the top, 0 the inside
    const BlockState cap = (red ? VanillaBlocks::RED_MUSHROOM_BLOCK() : VanillaBlocks::BROWN_MUSHROOM_BLOCK())
            .toBlockState();
    const int32_t top = y + height;
    for (int32_t layer = red ? top - 3 : top; layer <= top; layer++) {
        const int32_t radius = red ? (layer < top ? 2 : 1) : 3;
        const bool cutCorners = layer < top || !red;
        for (int32_t capX = x - radius; capX <= x + radius; capX++) {
            for (int32_t capZ = z - radius; capZ <= z + radius; capZ++) {
                const bool west = capX == x - radius;
                const bool east = capX == x + radius;
                const bool north = capZ == z - radius;
                const bool south = capZ == z + radius;
                int32_t bits = 5 + (west ? -1 : (east ? 1 : 0)) + (north ? -3 : (south ? 3 : 0));
                if (cutCorners) {
                    if ((west || east) && (north || south))
                        continue;
                    if ((capX == x - radius + 1 && north) || (west && capZ == z - radius + 1))
                        bits = 1;
                    if ((capX == x + radius - 1 && north) || (east && capZ == z - radius + 1))
                        bits = 3;
                    if ((capX == x - radius + 1 && south) || (west && capZ == z + radius - 1))
                        bits = 7;
                    if ((capX == x + radius - 1 && south) || (east && capZ == z + radius - 1))
                        bits = 9;
                }
                if (bits == 5 && layer < top)
                    bits = 0;
                if (bits == 0 || isSolidBlocking(blocks, capX, layer, capZ))
                    continue;

                blocks.setBlockStateAt(capX, layer, capZ, withState(cap, "huge_mushroom_bits", bits));
            }
        }
    }

    const BlockState stem = withState(VanillaBlocks::MUSHROOM_STEM().toBlockState(), "huge_mushroom_bits", 10);
    for (int32_t at = y; at < top; at++) {
        if (!isSolidBlocking(blocks, x, at, z))
            blocks.setBlockStateAt(x, at, z, stem);
    }
    return true;
}

bool OverworldRulePlacer::_placeLegacyFlower(BlockManager &blocks, const LegacyFlowerDefinition &flower, int32_t x,
                                             int32_t y, int32_t z, std::mt19937 &random) {
    if (!DecorationSupport::isAir(blocks.getBlockAt(x, y, z)))
        return false;

    // sub_14A293B00 picks the flower before the tries
    Block (*picked)() = &VanillaBlocks::POPPY;
    if (flower.mType == 1) {
        picked = nextBoolean(random) ? &VanillaBlocks::LILY_OF_THE_VALLEY : &VanillaBlocks::DANDELION;
    } else if (flower.mType == 2) {
        picked = nextInt(random, 3) == 0 ? &VanillaBlocks::DANDELION : &VanillaBlocks::POPPY;
    } else {
        // sub_14A2934C0: tulips where the 2345 simplex noise at a two hundredth of the position is under -0.8
        static Block (*const TULIPS[4])() = {&VanillaBlocks::RED_TULIP, &VanillaBlocks::ORANGE_TULIP,
                                             &VanillaBlocks::WHITE_TULIP, &VanillaBlocks::PINK_TULIP};
        static Block (*const OTHERS[4])() = {&VanillaBlocks::POPPY, &VanillaBlocks::AZURE_BLUET,
                                             &VanillaBlocks::OXEYE_DAISY, &VanillaBlocks::CORNFLOWER};
        const float noise = queryNoise(static_cast<float>(x) / 200.0f, static_cast<float>(z) / 200.0f);
        if (static_cast<double>(noise) < -0.8)
            picked = TULIPS[nextInt(random, 4)];
        else if (nextInt(random, 4) > 0)
            picked = OTHERS[nextInt(random, 4)];
        else
            picked = &VanillaBlocks::DANDELION;
    }

    // one try for worlds of 1.21.60 and later, at the position itself, where the flower may stand
    if (IFeature::isSupportDirt(blocks.getBlockAt(x, y - 1, z)))
        blocks.setBlockStateAt(x, y, z, picked().toBlockState());

    // the jitter of the next try is drawn after each one: (g & 7) - (g & 7) on z, (g & 3) - (g & 3) on y, x last
    for (int32_t draw = 0; draw < 6; draw++)
        random();
    return true;
}

bool OverworldRulePlacer::_placeDripstoneCluster(BlockManager &blocks, int32_t x, int32_t y, int32_t z,
                                                 std::mt19937 &random) {
    // the game's dripstone cluster (sub_1471E6910, built in sub_14726F340 by sub_14728CE10): height 3 to 5,
    // wetness a clamped normal of mean 0.1 and deviation 0.3 between 0.1 and 1, density 0.3 to 0.7, radius 2 to 7,
    // columns searched 12 blocks up and down; open blocks are air or water
    if (!isEmptyOrWater(blocks, x, y, z))
        return false;

    const int32_t height = 3 + nextInt(random, 3);
    float wetness = 0.1f + 0.3f * nextGaussian(random);
    wetness = 1.0f < wetness ? 1.0f : std::fmax(wetness, 0.1f);
    const float densityDraw = nextFloat(random);
    const int32_t radiusX = 2 + nextInt(random, 6);
    const int32_t radiusZ = 2 + nextInt(random, 6);
    const float density = (0.7f - 0.3f) * densityDraw + 0.3f;

    const auto open = [&](int32_t blockX, int32_t blockY, int32_t blockZ) {
        return isEmptyOrWater(blocks, blockX, blockY, blockZ);
    };
    const auto surface = [&](int32_t blockX, int32_t blockY, int32_t blockZ) {
        return isDripstoneBaseOrLava(blocks, blockX, blockY, blockZ);
    };
    // a pool may only lie in stone, deepslate, tuff or water
    const auto holdsWater = [&](int32_t blockX, int32_t blockY, int32_t blockZ) {
        static const BlockStateSet WALLS = {"minecraft:stone", "minecraft:deepslate", "minecraft:tuff"};
        const BlockState &state = blocks.getBlockAt(blockX, blockY, blockZ);
        return WALLS.contains(state) || isWater(state)
               || isWater(blocks.getExtraBlockAt(blockX, blockY, blockZ));
    };
    // DripstoneUtils' biased height: a gaussian of deviation 3 around half the height, lowered away from the
    // centre over 8 blocks
    const auto biased = [&](int32_t maxHeight, int32_t offsetX, int32_t offsetZ) {
        const float limit = static_cast<float>(maxHeight);
        const float half = limit * 0.5f;
        const float away = static_cast<float>(std::abs(offsetZ) + std::abs(offsetX)) * 0.125f;
        const float mean = 1.0f < away ? 0.0f : (0.0f - half) * away + half;
        const float value = nextGaussian(random) * 3.0f + mean;
        return static_cast<int32_t>(limit < value ? limit : std::fmax(value, 0.0f));
    };
    const auto countOpen = [&](int32_t blockX, int32_t fromY, int32_t blockZ, int32_t step, int32_t length) {
        int32_t count = 0;
        while (count < length && open(blockX, fromY + count * step, blockZ))
            count++;
        return count;
    };

    for (int32_t offsetX = -radiusX; offsetX <= radiusX; offsetX++) {
        for (int32_t offsetZ = -radiusZ; offsetZ <= radiusZ; offsetZ++) {
            const int32_t edge = std::min(radiusX - std::abs(offsetX), radiusZ - std::abs(offsetZ));
            const float ratio = static_cast<float>(edge) / 3.0f;
            const double columnChance = ratio < 0.0f ? static_cast<double>(0.1f)
                                                     : (ratio > 1.0f ? 1.0 : static_cast<double>(ratio * 0.9f + 0.1f));

            const int32_t columnX = x + offsetX;
            const int32_t columnZ = z + offsetZ;
            const ColumnEnd up = walkColumn(columnX, y, columnZ, FACE_STEPS[1], 12, open, surface);
            const ColumnEnd down = walkColumn(columnX, y, columnZ, FACE_STEPS[0], 12, open, surface);
            if (!up.mValid || (!up.mFound && !down.mFound))
                continue;

            const bool ceiling = up.mFound;
            const bool floor = down.mFound;
            const int32_t ceilingY = y + up.mSteps + 1;
            int32_t floorY = y - down.mSteps - 1;

            if (wetness > nextFloat(random) && floor) {
                const BlockState &floorState = blocks.getBlockAt(columnX, floorY, columnZ);
                if (!isDripstoneBlock(floorState) && !isPointedDripstone(floorState)
                    && !isWater(floorState) && !isWater(blocks.getExtraBlockAt(columnX, floorY, columnZ))
                    && !isWater(blocks.getBlockAt(columnX, floorY + 1, columnZ))
                    && !isWater(blocks.getExtraBlockAt(columnX, floorY + 1, columnZ))
                    && holdsWater(columnX, floorY, columnZ - 1) && holdsWater(columnX + 1, floorY, columnZ)
                    && holdsWater(columnX, floorY, columnZ + 1) && holdsWater(columnX - 1, floorY, columnZ)
                    && holdsWater(columnX, floorY - 1, columnZ)) {
                    static const BlockState water = VanillaBlocks::WATER().toBlockState();
                    blocks.setBlockStateAt(columnX, floorY, columnZ, water);
                    floorY--;
                }
            }

            int32_t stalactite = 0;
            if (columnChance > nextDouble(random) && ceiling && !isLava(blocks, columnX, ceilingY, columnZ)) {
                const int32_t layers = nextInt(random, 2) + 2;
                for (int32_t layer = 0; layer < layers; layer++) {
                    if (!placeDripstoneBlock(blocks, columnX, ceilingY + layer, columnZ))
                        break;
                }

                const int32_t maxHeight = floor ? std::min(height, ceilingY - floorY) : height;
                if (nextFloat(random) <= density)
                    stalactite = biased(maxHeight, offsetX, offsetZ);
            }

            int32_t stalagmite = 0;
            if (columnChance > nextDouble(random) && floor && !isLava(blocks, columnX, floorY, columnZ)) {
                const int32_t layers = nextInt(random, 2) + 2;
                for (int32_t layer = 0; layer < layers; layer++) {
                    if (!placeDripstoneBlock(blocks, columnX, floorY - layer, columnZ))
                        break;
                }

                if (ceiling) {
                    stalagmite = std::max(0, nextInt(random, 3) + stalactite - 1);
                } else if (nextFloat(random) <= density) {
                    stalagmite = biased(height, offsetX, offsetZ);
                }
            }

            if (ceiling) {
                // when the two would cross they meet at a drawn height between them
                if (floor && ceilingY - stalactite <= stalagmite + floorY) {
                    int32_t low = std::max(ceilingY - stalactite, floorY + 1);
                    const int32_t high = std::min(stalagmite + floorY, ceilingY - 1);
                    if (low <= high + 1)
                        low += nextInt(random, high - low + 2);
                    stalactite = ceilingY - low;
                    stalagmite = low - 1 - floorY;
                }
                stalactite = countOpen(columnX, ceilingY - 1, columnZ, -1, stalactite);
            }
            if (floor)
                stalagmite = countOpen(columnX, floorY + 1, columnZ, 1, stalagmite);

            const bool merge = nextBoolean(random) && stalactite > 0 && stalagmite > 0 && floor && ceiling
                               && stalactite + stalagmite == ceilingY - floorY - 1;
            if (ceiling)
                growPointedDripstone(blocks, columnX, ceilingY - 1, columnZ, false, stalactite, merge);
            if (floor)
                growPointedDripstone(blocks, columnX, floorY + 1, columnZ, true, stalagmite, merge);
        }
    }
    return true;
}

bool OverworldRulePlacer::_placePointedDripstone(BlockManager &blocks, int32_t x, int32_t y, int32_t z,
                                                 std::mt19937 &random) {
    // the game's pointed dripstone (sub_1471E82A0): grows from the base below or above, a patch of dripstone blocks
    // around that base, then one or two blocks long
    const auto isBase = [&](int32_t blockY) {
        const BlockState &state = blocks.getBlockAt(x, blockY, z);
        return isDripstoneReplaceable(state) || isDripstoneBlock(state);
    };
    const bool below = isBase(y - 1);
    const bool above = isBase(y + 1);
    bool up = false;
    if (below && above)
        up = nextBoolean(random);
    else if (below)
        up = true;
    else if (!above)
        return false;

    const int32_t baseY = up ? y - 1 : y + 1;
    placeDripstoneBlock(blocks, x, baseY, z);
    // down, up, north, south, west then east, each spreading up to two more blocks in drawn faces
    for (const int32_t *step: FACE_STEPS) {
        if (nextFloat(random) < 0.3f)
            continue;

        int32_t spreadX = x + step[0];
        int32_t spreadY = baseY + step[1];
        int32_t spreadZ = z + step[2];
        placeDripstoneBlock(blocks, spreadX, spreadY, spreadZ);
        for (int32_t further = 0; further < 2; further++) {
            if (nextBoolean(random))
                break;

            const int32_t *drawn = FACE_STEPS[nextInt(random, 6)];
            spreadX += drawn[0];
            spreadY += drawn[1];
            spreadZ += drawn[2];
            placeDripstoneBlock(blocks, spreadX, spreadY, spreadZ);
        }
    }

    const float taller = nextFloat(random);
    const int32_t nextY = up ? y + 1 : y - 1;
    const int32_t length = isEmptyOrWater(blocks, x, nextY, z) && taller < 0.2f ? 2 : 1;
    growPointedDripstone(blocks, x, y, z, up, length, false);
    return true;
}

bool OverworldRulePlacer::_placeLargeDripstone(BlockManager &blocks, int32_t x, int32_t y, int32_t z,
                                               std::mt19937 &random) {
    // the game's large dripstone (sub_14D21B810, vtable 0x14F4A1470): radius 3 to 18 at most a third of the cave,
    // stalactite bluntness 0.3 to 0.9, stalagmite bluntness 0.4 to 1, height scale 0.4 to 2, a wind of 0 to 0.3
    // from radius 4 and bluntness 0.6
    struct LargeDripstone {
        int32_t mX;
        int32_t mY;
        int32_t mZ;
        bool mUp;
        int32_t mRadius;
        float mBluntness;
        float mScale;

        float heightAt(float distance) const {
            return largeDripstoneHeight(distance, static_cast<float>(mRadius), mScale, mBluntness);
        }
    };
    struct Wind {
        bool mBlows = false;
        int32_t mOriginY = 0;
        float mX = 0.0f;
        float mZ = 0.0f;
    };

    if (!isEmptyOrWater(blocks, x, y, z))
        return false;

    const auto open = [&](int32_t blockX, int32_t blockY, int32_t blockZ) {
        return blockY <= LevelChunk::MAX_Y + 1 && blockY >= LevelChunk::MIN_Y
               && isEmptyOrWater(blocks, blockX, blockY, blockZ);
    };
    const auto surface = [&](int32_t blockX, int32_t blockY, int32_t blockZ) {
        return isDripstoneBaseOrLava(blocks, blockX, blockY, blockZ);
    };
    const ColumnEnd up = walkColumn(x, y, z, FACE_STEPS[1], 30, open, surface);
    const ColumnEnd down = walkColumn(x, y, z, FACE_STEPS[0], 30, open, surface);
    if (!up.mValid || !up.mFound || !down.mFound)
        return false;

    const int32_t ceilingY = y + up.mSteps + 1;
    const int32_t floorY = y - down.mSteps - 1;
    if (floorY - ceilingY >= -4)
        return false;

    const int32_t widest = static_cast<int32_t>(static_cast<float>(ceilingY - floorY - 1) * 0.33f);
    const int32_t maxRadius = widest > 18 ? 18 : std::max(widest, 3);
    const int32_t radius = 3 + nextInt(random, maxRadius - 3 + 1);
    const float hangingBluntness = nextFloat(random) * (0.9f - 0.3f) + 0.3f;
    const float hangingScale = nextFloat(random) * (2.0f - 0.4f) + 0.4f;
    const float standingBluntness = nextFloat(random) * (1.0f - 0.4f) + 0.4f;
    const float standingScale = nextFloat(random) * (2.0f - 0.4f) + 0.4f;
    LargeDripstone hanging = {x, ceilingY - 1, z, false, radius, hangingBluntness, hangingScale};
    LargeDripstone standing = {x, floorY + 1, z, true, radius, standingBluntness, standingScale};

    Wind wind;
    if (radius >= 4 && hangingBluntness >= 0.6f && standingBluntness >= 0.6f) {
        const float speed = (0.3f - 0.0f) * nextFloat(random) + 0.0f;
        const float angle = (nextFloat(random) * PI + 0.0f) * 10430.378f;
        wind.mBlows = true;
        wind.mOriginY = y;
        wind.mX = FeatureMath::sineAt(angle + 16384.0f) * speed;
        wind.mZ = FeatureMath::sineAt(angle) * speed;
    }
    const auto blown = [&](int32_t &blockX, int32_t blockY, int32_t &blockZ) {
        if (!wind.mBlows)
            return;

        const float rise = static_cast<float>(wind.mOriginY - blockY);
        blockZ += static_cast<int32_t>(wind.mZ * rise);
        blockX += static_cast<int32_t>(rise * wind.mX);
    };
    const auto openOrLava = [&](int32_t blockX, int32_t blockY, int32_t blockZ) {
        return isEmptyOrWater(blocks, blockX, blockY, blockZ) || isLava(blocks, blockX, blockY, blockZ);
    };

    // sub_14721C980: the block and a ring of the radius around it are all solid
    const auto embedded = [&](int32_t blockX, int32_t blockY, int32_t blockZ, int32_t ringRadius) {
        if (openOrLava(blockX, blockY, blockZ))
            return false;

        const float ring = static_cast<float>(ringRadius);
        const auto step = static_cast<float>(6.0 / static_cast<double>(ring));
        for (float angle = 0.0f; angle < 6.2831855f; angle += step) {
            const float scaled = angle * 10430.378f;
            const int32_t ringX = blockX + static_cast<int32_t>(FeatureMath::sineAt(scaled + 16384.0f) * ring);
            const int32_t ringZ = blockZ + static_cast<int32_t>(FeatureMath::sineAt(scaled) * ring);
            if (openOrLava(ringX, blockY, ringZ))
                return false;
        }
        return true;
    };

    // sub_14D21BE40: the base moved into the rock, the radius halved while it does not fit
    const auto settle = [&](LargeDripstone &dripstone) {
        while (dripstone.mRadius >= 2) {
            const auto height = static_cast<int32_t>(dripstone.heightAt(0.0f));
            const int32_t steps = std::min(height, 10);
            for (int32_t i = 0; i < steps; i++) {
                const int32_t blockY = dripstone.mY + (dripstone.mUp ? -i : i);
                if (isLava(blocks, dripstone.mX, blockY, dripstone.mZ))
                    return false;

                int32_t blockX = dripstone.mX;
                int32_t blockZ = dripstone.mZ;
                blown(blockX, blockY, blockZ);
                if (embedded(blockX, blockY, blockZ, dripstone.mRadius)) {
                    dripstone.mY = blockY;
                    return true;
                }
            }

            const int32_t previous = dripstone.mRadius;
            dripstone.mRadius = previous / 2;
            if (previous <= 3)
                return false;
        }
        return false;
    };

    // sub_14D21C170: column by column, a stalagmite no higher than the generation heightmap; speleothems are
    // replaced, and a column ends at the first other solid block once it has placed one
    static const BlockState dripstoneBlock = VanillaBlocks::DRIPSTONE_BLOCK().toBlockState();
    const auto placeBlocks = [&](const LargeDripstone &dripstone) {
        const int32_t r = dripstone.mRadius;
        for (int32_t offsetX = -r; offsetX <= r; offsetX++) {
            for (int32_t offsetZ = -r; offsetZ <= r; offsetZ++) {
                const float distance = std::sqrt(static_cast<float>(offsetX * offsetX + offsetZ * offsetZ));
                if (distance > static_cast<float>(r))
                    continue;

                auto height = static_cast<int32_t>(dripstone.heightAt(distance));
                if (height <= 0)
                    continue;
                if (static_cast<double>(nextFloat(random)) < 0.2)
                    height = static_cast<int32_t>((nextFloat(random) * 0.19999999f + 0.8f)
                                                  * static_cast<float>(height));

                const int32_t columnX = dripstone.mX + offsetX;
                const int32_t columnZ = dripstone.mZ + offsetZ;
                const int32_t limit = dripstone.mUp ? heightmapAt(blocks.getLevel(), columnX, columnZ) : INT32_MAX;
                int32_t blockY = dripstone.mY;
                if (height <= 0 || blockY >= limit)
                    continue;

                bool placed = false;
                for (int32_t i = 0;;) {
                    int32_t blockX = columnX;
                    int32_t blockZ = columnZ;
                    blown(blockX, blockY, blockZ);
                    static const BlockStateSet SPIKES = {
                            "minecraft:pointed_dripstone", "minecraft:sulfur_spike", "minecraft:icicle"
                    };
                    if (openOrLava(blockX, blockY, blockZ)
                        || SPIKES.contains(blocks.getBlockAt(blockX, blockY, blockZ))) {
                        blocks.setBlockStateAt(blockX, blockY, blockZ, dripstoneBlock);
                        placed = true;
                    } else if (placed) {
                        break;
                    }

                    if (++i >= height)
                        break;
                    blockY += dripstone.mUp ? 1 : -1;
                    if (blockY >= limit)
                        break;
                }
            }
        }
    };

    const bool hangingFits = settle(hanging);
    const bool standingFits = settle(standing);
    bool placed = false;
    if (hangingFits
        && hanging.mY - static_cast<int32_t>(hanging.heightAt(0.0f)) > LevelChunk::MIN_Y) {
        placeBlocks(hanging);
        placed = true;
    }
    if (standingFits
        && standing.mY + static_cast<int32_t>(standing.heightAt(0.0f)) < LevelChunk::MAX_Y + 1) {
        placeBlocks(standing);
        placed = true;
    }
    return placed;
}

bool OverworldRulePlacer::_placeMultiface(BlockManager &blocks, const MultifaceDefinition &multiface, int32_t x,
                                          int32_t y, int32_t z, std::mt19937 &random) {
    // the game shuffles the faces with a thread local random seeded from the system (sub_140391AB0), so which face
    // it tries first, and where it spreads, cannot be reproduced: the faces are tried in their listed order
    const BlockState &start = blocks.getBlockAt(x, y, z);
    if (!DecorationSupport::isAir(start) && !isWater(start))
        return false;

    if (_growMultiface(blocks, multiface, x, y, z, multiface.mFaces, random))
        return true;

    const std::string placedName = multiface.mPlaces().toBlockState().mName;
    for (int32_t face: multiface.mFaces) {
        std::vector<int32_t> others;
        for (int32_t other: multiface.mFaces) {
            if (other != (face ^ 1))
                others.push_back(other);
        }

        // every step of the search range offsets the start once, so they all try the same block
        const int32_t nextX = x + FACE_STEPS[face][0];
        const int32_t nextY = y + FACE_STEPS[face][1];
        const int32_t nextZ = z + FACE_STEPS[face][2];
        const BlockState &next = blocks.getBlockAt(nextX, nextY, nextZ);
        if (multiface.mSearchRange <= 0
            || (!DecorationSupport::isAir(next) && !isWater(next) && next.mName != placedName))
            continue;
        if (_growMultiface(blocks, multiface, nextX, nextY, nextZ, others, random))
            return true;
    }
    return false;
}

bool OverworldRulePlacer::_growMultiface(BlockManager &blocks, const MultifaceDefinition &multiface, int32_t x,
                                         int32_t y, int32_t z, const std::vector<int32_t> &faces,
                                         std::mt19937 &random) {
    // sub_14A2CDE20: the first face whose neighbour it may grow on, the bits in down, up, south, west, north, east
    // order
    static const int32_t BITS[6] = {1, 2, 16, 4, 8, 32};
    int32_t chosen = -1;
    for (int32_t face: faces) {
        if (contains(multiface.mCanPlaceOn, blocks.getBlockAt(x + FACE_STEPS[face][0], y + FACE_STEPS[face][1],
                                                               z + FACE_STEPS[face][2]))) {
            chosen = face;
            break;
        }
    }
    if (chosen < 0)
        return false;

    const BlockState placedState = multiface.mPlaces().toBlockState();
    const BlockState &current = blocks.getBlockAt(x, y, z);
    const int32_t bits = current.mName == placedState.mName
                         ? current.mStates.getInt("multi_face_direction_bits", 0) : 0;
    if ((bits | BITS[chosen]) == bits)
        return false;

    setInWater(blocks, x, y, z, withState(placedState, "multi_face_direction_bits", bits | BITS[chosen]));
    // the spread this draw allows picks its block with the same thread local random, and is left out
    nextFloat(random);
    return true;
}

bool OverworldRulePlacer::_placeThreshold(BlockManager &blocks, const ThresholdDefinition &threshold, int32_t x,
                                          int32_t y, int32_t z, std::mt19937 &random) {
    if (placementContext.mClimate == nullptr)
        return false;

    // the generator's preliminary surface of the quart column, not the blocks
    const auto surface = static_cast<int16_t>(placementContext.mClimate->preliminarySurfaceLevel(x >> 2, z >> 2));
    if (surface - threshold.mMinimumDistanceBelowSurface <= y)
        return false;

    return _place(blocks, threshold.mPlaces, x, y, z, random);
}

bool OverworldRulePlacer::_placeSnap(BlockManager &blocks, const SnapDefinition &snap, int32_t x, int32_t y, int32_t z,
                                     std::mt19937 &random) {
    int32_t snapped[3] = {x, y, z};
    bool found = false;
    int32_t surface = snap.mSurface;
    if (surface == 2) {
        // north, east, south and west, shuffled with raw draws
        int32_t faces[4] = {2, 5, 3, 4};
        if ((random() & 1u) == 0) {
            faces[0] = 5;
            faces[1] = 2;
        }
        const uint32_t third = random() % 3u;
        if (third != 2)
            std::swap(faces[third], faces[2]);
        const uint32_t fourth = random() & 3u;
        if (fourth != 3)
            std::swap(faces[fourth], faces[3]);

        for (int32_t face: faces) {
            found = _snapAlong(blocks, snap, x, y, z, face, snapped);
            if (found)
                break;
        }
    } else {
        // random_horizontal picks the ceiling or the floor, with nextBoolean's bit 27
        if (surface == 3)
            surface = (random() & 0x8000000u) != 0 ? 0 : 1;

        const BlockState &start = blocks.getBlockAt(x, y, z);
        const bool open = DecorationSupport::isAir(start) || isWater(start);
        const int32_t face = open ? (surface == 0 ? 1 : 0) : (surface != 0 ? 1 : 0);
        found = _snapAlong(blocks, snap, x, y, z, face, snapped);
    }

    if (!found)
        return false;
    return _place(blocks, snap.mPlaces, snapped[0], snapped[1], snapped[2], random);
}

bool OverworldRulePlacer::_snapAlong(BlockManager &blocks, const SnapDefinition &snap, int32_t x, int32_t y, int32_t z,
                                     int32_t face, int32_t snapped[3]) {
    // sub_14A271850: from an open start along the face while the blocks stay open, to a surface that supports it;
    // from a solid start while they stay solid, to an open block
    const auto isOpen = [&](const BlockState &state) {
        return (snap.mAllowAirPlacement && DecorationSupport::isAir(state))
               || (snap.mAllowUnderwaterPlacement && isWater(state));
    };
    const auto isNonAir = [&](const BlockState &state) {
        return snap.mAllowNonAirPlacement && !DecorationSupport::isAir(state) && !isWater(state);
    };

    const BlockState &start = blocks.getBlockAt(x, y, z);
    const bool solidStart = !DecorationSupport::isAir(start) && !isWater(start);
    const LevelChunk *startChunk = blocks.getLevel().peekChunkPtr(x >> 4, z >> 4);
    if (startChunk == nullptr)
        return false;

    // the walk stays in the start's biome (BlockSource vfunc 192)
    const uint32_t biome = startChunk->getBiomeAt(x & 15, y, z & 15);
    const auto continues = [&](int32_t blockX, int32_t blockY, int32_t blockZ) {
        const LevelChunk *chunk = blocks.getLevel().peekChunkPtr(blockX >> 4, blockZ >> 4);
        if (chunk == nullptr || blockY < LevelChunk::MIN_Y || blockY > LevelChunk::MAX_Y
            || chunk->getBiomeAt(blockX & 15, blockY, blockZ & 15) != biome)
            return false;

        const BlockState &state = blocks.getBlockAt(blockX, blockY, blockZ);
        return solidStart ? isNonAir(state) : isOpen(state);
    };
    // the surface is a full block, the game's face support of any type (sub_14A2A0010) approximated
    const auto ends = [&](int32_t blockX, int32_t blockY, int32_t blockZ) {
        const BlockState &end = blocks.getBlockAt(blockX, blockY, blockZ);
        if (!snap.mAllowedSurfaceBlocks.empty())
            return contains(snap.mAllowedSurfaceBlocks, end);
        return solidStart ? isOpen(end) : BlockSupport::isSolid(end);
    };

    const int32_t *step = FACE_STEPS[face];
    const ColumnEnd end = walkColumn(x, y, z, step, snap.mVerticalSearchRange, continues, ends);
    if (!end.mFound)
        return false;

    const int32_t distance = end.mSteps + ((snap.mEmbedInSurface != solidStart) ? 1 : 0);
    snapped[0] = x + step[0] * distance;
    snapped[1] = y + step[1] * distance;
    snapped[2] = z + step[2] * distance;
    return true;
}

bool OverworldRulePlacer::_placeBlob(BlockManager &blocks, const BlobDefinition &blob, int32_t x, int32_t y, int32_t z,
                                     std::mt19937 &random) {
    // around the block under the position, nearest first (sub_142D32730), each one drawn before it is checked;
    // a block and its neighbours but the exposed face must be neither air nor water, on either layer
    const auto isClosed = [&](int32_t blockX, int32_t blockY, int32_t blockZ) {
        const BlockState &state = blocks.getBlockAt(blockX, blockY, blockZ);
        return !isWater(state) && !DecorationSupport::isAir(state)
               && !isWater(blocks.getExtraBlockAt(blockX, blockY, blockZ));
    };

    const BlockState state = blob.mPlaces().toBlockState();
    const int32_t centerY = y - 1;
    const int32_t radius = blob.mPlacementRadiusAroundFloor;
    int32_t placed = 0;
    // BlockPos within Manhattan distance: depth by depth, x then y rising, z on both sides
    for (int32_t depth = 0; depth <= 3 * radius; depth++) {
        const int32_t maxX = std::min(radius, depth);
        for (int32_t offsetX = -maxX; offsetX <= maxX; offsetX++) {
            const int32_t maxY = std::min(radius, depth - std::abs(offsetX));
            for (int32_t offsetY = -maxY; offsetY <= maxY; offsetY++) {
                const int32_t offsetZ = depth - std::abs(offsetX) - std::abs(offsetY);
                if (offsetZ > radius)
                    continue;

                for (int32_t side = 0; side < (offsetZ != 0 ? 2 : 1); side++) {
                    const int32_t blockX = x + offsetX;
                    const int32_t blockY = centerY + offsetY;
                    const int32_t blockZ = z + (side == 0 ? offsetZ : -offsetZ);
                    if (nextFloat(random) > blob.mPlacementProbabilityPerValidPosition
                        || !isClosed(blockX, blockY, blockZ))
                        continue;

                    bool covered = true;
                    for (int32_t face = 0; face < 6 && covered; face++) {
                        if (face != blob.mExposedFace)
                            covered = isClosed(blockX + FACE_STEPS[face][0], blockY + FACE_STEPS[face][1],
                                               blockZ + FACE_STEPS[face][2]);
                    }
                    if (!covered)
                        continue;

                    blocks.setBlockStateAt(blockX, blockY, blockZ, state);
                    placed++;
                }
            }
        }
    }
    return placed != 0;
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
    const BlockState &support = blocks.getBlockAt(x, y - 1, z);
    const bool supported = IFeature::isSupportDirt(support)
                           || (single.mNetherPlant && (support.mName == "minecraft:crimson_nylium"
                                                       || support.mName == "minecraft:warped_nylium"
                                                       || support.mName == "minecraft:soul_soil"));
    if (single.mSurvival && !supported)
        return false;
    if (single.mDoublePlant && (!supported || !DecorationSupport::isAir(blocks.getBlockAt(x, y + 1, z))))
        return false;

    static const BlockStateSet SHORT_GRASS = {VanillaBlocks::SHORT_GRASS().getIdentifier()};
    const BlockState &replaced = blocks.getBlockAt(x, y, z);
    if (!DecorationSupport::isAir(replaced) && !(single.mReplacesShortGrass && SHORT_GRASS.contains(replaced)))
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
               && !DecorationSupport::isAir(blocks.getBlockAt(plantX, plantY, plantZ)))
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
            if (isKelp(blocks.getBlockAt(x, baseY, z)) || baseY <= LevelChunk::MIN_Y)
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
               && !DecorationSupport::isAir(blocks.getBlockAt(plantX, plantY, plantZ)))
            plantY++;

        const BlockState &at = blocks.getBlockAt(plantX, plantY, plantZ);
        if (DecorationSupport::isAir(at) || isWater(blocks.getBlockAt(plantX, plantY - 1, plantZ))
            || isKelp(at))
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
                              && (isKelp(below) || isSolidBlocking(blocks, plantX, plantY - 1, plantZ));
        if (!isKelp(below) && mayPlace) {
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
            || (!DecorationSupport::isAir(extra) && refusesGeodePoint(extra))) {
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
        if (!DecorationSupport::isAir(blocks.getExtraBlockAt(blockX, blockY, blockZ)))
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
            if ((!DecorationSupport::isAir(replaced) && !isWater(replaced))
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
    // below 1 one tree with that chance; otherwise the density scaled by a float; only the swamp's seagrass reads the
    // rule's y
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
    // the grass it scatters around each tree outside legacy worlds (sub_145E2AA10) has 0 iterations
    if (trees.mSeagrass != nullptr) {
        for (int32_t attempt = 0; attempt < 10; attempt++) {
            const int32_t seagrassX = x + nextInt(random, 16);
            const int32_t seagrassZ = z + nextInt(random, 16);
            _placeSeagrass(blocks, *trees.mSeagrass, seagrassX, y + 54, seagrassZ, random);
        }
    }

    using Pick = LegacyTreeDefinition::Pick;
    for (int32_t tree = 0; tree < count; tree++) {
        size_t picked = 0;
        if (trees.mPick == Pick::Forest)
            picked = nextInt(random, 5) != 0 ? (nextInt(random, 10) != 0 ? 2 : 1) : 0;
        else if (trees.mPick == Pick::OneInThree)
            picked = nextInt(random, 3) == 0 ? 0 : 1;
        else if (trees.mPick == Pick::Swamp)
            picked = nextInt(random, 20) != 0 ? 1 : 0;
        else if (trees.mPick == Pick::BirchMutated)
            picked = nextInt(random, 5) != 0 && nextBoolean(random) ? 1 : 0;
        else if (trees.mPick == Pick::MegaTaiga)
            picked = nextInt(random, 3) != 0 ? (nextInt(random, 3) != 0 ? 3 : 2) : (nextInt(random, 13) != 0 ? 1 : 0);

        const int32_t treeZ = z + nextInt(random, 14) + 1;
        const int32_t treeX = x + nextInt(random, 14) + 1;
        if (picked >= trees.mFeatures.size())
            continue;
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
    return OverworldTreePlacer::place(blocks, tree.mTree, tree.mFancy, tree.mMega, x, y, z, random,
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
    if (!DecorationSupport::isAir(blocks.getBlockAt(x, y, z)) || !BlockSupport::isSolid(blocks.getBlockAt(x, y - 1, z)))
        return false;

    const BlockState litter = withState(withState(VanillaBlocks::LEAF_LITTER().toBlockState(), "growth", growth),
                                        "minecraft:cardinal_direction", DIRECTIONS[direction]);
    blocks.setBlockStateAt(x, y, z, litter);
    return true;
}

bool OverworldRulePlacer::_placeMushroom(BlockManager &blocks, Block (*mushroom)(), int32_t x, int32_t y, int32_t z) {
    if (!DecorationSupport::isAir(blocks.getBlockAt(x, y, z)) || !BlockSupport::isSolid(blocks.getBlockAt(x, y - 1, z)))
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
    // the may_attach_to blocks of beehive_feature
    static const BlockStateSet TRUNKS = {
            "minecraft:oak_log", "minecraft:birch_log", "minecraft:mangrove_roots", "minecraft:cherry_log"
    };
    static const BlockStateSet LEAVES = {
            "minecraft:oak_leaves", "minecraft:birch_leaves", "minecraft:mangrove_leaves", "minecraft:cherry_leaves"
    };
    const auto isTrunk = [&](int32_t blockX, int32_t blockY, int32_t blockZ) {
        return TRUNKS.contains(blocks.getBlockAt(blockX, blockY, blockZ));
    };
    for (int32_t offsetY = 0; offsetY <= 12; offsetY++) {
        const int32_t order = nextInt(random, 3);
        for (const int32_t *side: ORDERS[order]) {
            nextInt(random, 1);
            const int32_t nestX = x + side[0];
            const int32_t nestY = y + offsetY;
            const int32_t nestZ = z + side[1];
            if (!DecorationSupport::isAir(blocks.getBlockAt(nestX, nestY, nestZ))
                || !DecorationSupport::isAir(blocks.getBlockAt(nestX, nestY, nestZ + 1)))
                continue;

            const int32_t attached = LEAVES.contains(blocks.getBlockAt(nestX, nestY + 1, nestZ))
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

void OverworldRulePlacer::_placeSprings(BlockManager &blocks, int32_t x, int32_t z, std::mt19937 &random) {
    // LegacySpringsFeature::place (sub_14D21E680) for worlds of the 1.18 terrain: 25 water springs anywhere from
    // the bottom of the world up to 192, then 20 lava springs biased towards the bottom, kept out of the deep dark;
    // z, then y, then x
    static const BlockState water = VanillaBlocks::WATER().toBlockState();
    static const BlockState lava = VanillaBlocks::LAVA().toBlockState();
    for (int32_t spring = 0; spring < 25; spring++) {
        const int32_t offsetZ = nextInt(random, 16);
        const int32_t y = LevelChunk::MIN_Y + nextInt(random, 192 - LevelChunk::MIN_Y);
        const int32_t offsetX = nextInt(random, 16);
        _placeSpring(blocks, x + offsetX, y, z + offsetZ, water);
    }

    for (int32_t spring = 0; spring < 20; spring++) {
        const int32_t offsetZ = nextInt(random, 16);
        const int32_t y = LevelChunk::MIN_Y + 8
                          + nextInt(random, nextInt(random, nextInt(random, 192 - 16 - LevelChunk::MIN_Y)));
        const int32_t offsetX = nextInt(random, 16);
        const int32_t springX = x + offsetX;
        const int32_t springZ = z + offsetZ;
        const LevelChunk *chunk = blocks.getLevel().peekChunkPtr(springX >> 4, springZ >> 4);
        if (chunk == nullptr || y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y
            || chunk->getBiomeAt(springX & 15, y, springZ & 15) == BiomeIds::DEEP_DARK)
            continue;

        _placeSpring(blocks, springX, y, springZ, lava);
    }
}

void OverworldRulePlacer::_placeSpring(BlockManager &blocks, int32_t x, int32_t y, int32_t z,
                                       const BlockState &liquid) {
    // the spring feature (sub_1471E9B40): rock above and below, four rock sides around one opening
    const auto isRock = [&](int32_t blockX, int32_t blockY, int32_t blockZ) {
        // sub_1471E9FF0 for worlds of the 1.18 terrain
        static const std::vector<std::string> ROCK = {
                "minecraft:stone", "minecraft:deepslate", "minecraft:tuff", "minecraft:calcite", "minecraft:snow",
                "minecraft:powder_snow", "minecraft:packed_ice", "minecraft:dirt", "minecraft:coarse_dirt"
        };
        return contains(ROCK, blocks.getBlockAt(blockX, blockY, blockZ));
    };
    const auto isAir = [&](int32_t blockX, int32_t blockY, int32_t blockZ) {
        return DecorationSupport::isAir(blocks.getBlockAt(blockX, blockY, blockZ));
    };

    if (!isRock(x, y + 1, z) || !isRock(x, y - 1, z) || !(isAir(x, y, z) || isRock(x, y, z)))
        return;

    const int32_t rock = isRock(x - 1, y, z) + isRock(x + 1, y, z) + isRock(x, y, z - 1) + isRock(x, y, z + 1)
                         + isRock(x, y - 1, z);
    const int32_t open = isAir(x - 1, y, z) + isAir(x + 1, y, z) + isAir(x, y, z - 1) + isAir(x, y, z + 1)
                         + isAir(x, y - 1, z);
    if (rock == 4 && open == 1)
        blocks.setBlockStateAt(x, y, z, liquid);
}

void OverworldRulePlacer::_placeLegacyIce(BlockManager &blocks, int32_t x, int32_t z, std::mt19937 &random) {
    // LegacyIceFeature::place (sub_14D21DBE0): one chunk in eight an iceberg 12 to 19 blocks into it, then up to 19
    // blue ice patches 30 to 61 high
    if (nextInt(random, 8) == 0) {
        const int32_t offsetX = nextInt(random, 8);
        const int32_t offsetZ = nextInt(random, 8);
        _placeIceberg(blocks, x + offsetX + 12, z + offsetZ + 12, random);
    }

    const int32_t patches = nextInt(random, 20);
    for (int32_t patch = 0; patch < patches; patch++) {
        const int32_t offsetX = nextInt(random, 16);
        const int32_t y = nextInt(random, 32) + 30;
        const int32_t offsetZ = nextInt(random, 16);
        _placeBlueIce(blocks, x + offsetX + 8, y, z + offsetZ + 8, random);
    }
}

void OverworldRulePlacer::_placeIceberg(BlockManager &blocks, int32_t x, int32_t z, std::mt19937 &random) {
    // the game's iceberg feature (sub_14D215C00, vtable 0x14F4A0F30) at sea level: a round iceberg, or one in five an
    // elliptic one, its part under the sea, then smoothed, and most of the time a cut through it (sub_14D217330);
    // in floats, with the game's own draws
    static const BlockState air = VanillaBlocks::AIR().toBlockState();
    static const BlockState water = VanillaBlocks::WATER().toBlockState();
    static const BlockState snow = VanillaBlocks::SNOW().toBlockState();
    static const BlockState packedIce = VanillaBlocks::PACKED_ICE().toBlockState();
    static const BlockState blueIce = VanillaBlocks::BLUE_ICE().toBlockState();
    constexpr int32_t SEA_LEVEL = 63;

    static const BlockStateSet ICEBERG = {"minecraft:packed_ice", "minecraft:snow", "minecraft:blue_ice"};
    static const BlockStateSet SNOW_LAYER = {VanillaBlocks::SNOW_LAYER().getIdentifier()};
    const auto isIceberg = [](const BlockState &state) {
        return ICEBERG.contains(state);
    };

    const int32_t widthDraw = nextInt(random, 5);
    const int32_t depthDraw = nextInt(random, 3);
    const int32_t ellipseHeightDraw = nextInt(random, 6);
    const int32_t roundHeightDraw = nextInt(random, 15);
    const bool ellipse = nextFloat(random) > 0.8f;
    const int32_t ellipseWidth = 11 - widthDraw;
    int32_t height = ellipseHeightDraw + 6;
    int32_t width = ellipseWidth;
    if (!ellipse) {
        width = 11;
        height = nextFloat(random) <= 0.9f ? roundHeightDraw + 3 : roundHeightDraw + nextInt(random, 19) + 10;
    }
    const int32_t ellipseDepth = depthDraw + 3;
    const bool snowOnTop = nextFloat(random) > 0.7f;
    const float angleDraw = nextFloat(random);
    const float angle = (angleDraw + angleDraw) * PI;
    const int32_t radiusDraw = nextInt(random, 7);
    const int32_t radiusValue = std::min(height + radiusDraw - nextInt(random, 5), 11);
    const BlockState &main = nextFloat(random) <= 0.01f ? blueIce : packedIce;

    const float heightF = static_cast<float>(height);
    const float radiusF = static_cast<float>(radiusValue);
    const auto radiusD = static_cast<double>(radiusValue);
    const float cosine = FeatureMath::sineAt(10430.378f * angle + 16384.0f);
    const float sine = FeatureMath::sineAt(10430.378f * angle);

    const auto ceilOf = [](float value) {
        const auto truncated = static_cast<int32_t>(value);
        return static_cast<float>(truncated) < value ? truncated + 1 : truncated;
    };
    // the radius of a round iceberg at a height, two shapes above 15 blocks
    const auto roundRadius = [&](int32_t dy, float dyF) {
        const auto spread = static_cast<float>(3.5 - static_cast<double>(nextFloat(random))) * heightF;
        float radius = 0.0f;
        if (height <= nextInt(random, 5) + 15) {
            radius = static_cast<float>((1.0 - static_cast<double>(dyF * dyF / spread)) * radiusD);
        } else {
            const int32_t low = nextInt(random, 6) + 3;
            radius = (static_cast<float>(dy >> (dy < low ? 1 : 0)) / (spread * -0.4f) + 1.0f) * radiusF;
        }
        return ceilOf(radius * 0.5f);
    };
    // the radius under the sea, steeper
    const auto steepRadius = [&](int32_t below, float total) {
        const float steep = static_cast<float>(below) / ((nextFloat(random) * 0.5f + 1.0f) * total);
        return ceilOf((1.0f - steep) * radiusF * 0.5f);
    };
    // a round iceberg's distance is jittered by a draw
    const auto circleDistance = [&](double distanceSquared, int32_t radius) {
        const float jitter = nextFloat(random);
        const float offset = (0.8f < jitter ? 8.0f : std::fmax(jitter, 0.2f) * 10.0f) / static_cast<float>(radius);
        return distanceSquared - static_cast<double>(radius) * static_cast<double>(radius) + offset;
    };
    const auto ellipseDistance = [&](float dxF, float dzF, float a, float b, float rotationCos, float rotationSin) {
        const auto first = static_cast<double>((rotationCos * dxF - rotationSin * dzF) / a);
        const auto second = static_cast<double>((rotationCos * dzF + rotationSin * dxF) / b);
        return second * second + first * first + -1.0;
    };
    // only in air, snow, ice or water; snow near the top when the iceberg carries some
    const auto setIcebergBlock = [&](int32_t blockX, int32_t blockY, int32_t blockZ, int32_t fromTop, int32_t total) {
        static const BlockStateSet TAKES_ICEBERG = {
                "minecraft:air", "minecraft:snow", "minecraft:ice", "minecraft:water", "minecraft:flowing_water"
        };
        static const BlockStateSet STILL_WATER = {VanillaBlocks::WATER().getIdentifier()};
        const BlockState &replaced = blocks.getBlockAt(blockX, blockY, blockZ);
        if (!TAKES_ICEBERG.contains(replaced))
            return;

        bool allowSnow = true;
        int32_t divisor = 2;
        if (ellipse) {
            allowSnow = static_cast<double>(nextFloat(random)) > 0.05;
            divisor = 3;
        }

        const BlockState *state = &main;
        if (snowOnTop && !STILL_WATER.contains(replaced)) {
            const int32_t drawn = nextInt(random, std::max(1, total / divisor));
            if (static_cast<double>(drawn) + static_cast<double>(total) * 0.6 >= static_cast<double>(fromTop)
                && allowSnow)
                state = &snow;
        }
        blocks.setBlockStateAt(blockX, blockY, blockZ, *state);
    };

    // above the sea
    if (width > 0 && height > 0) {
        for (int32_t dx = -width; dx < width; dx++) {
            const double dxSquared = static_cast<double>(dx) * static_cast<double>(dx);
            for (int32_t dz = -width; dz < width; dz++) {
                const double distanceSquared = static_cast<double>(dz) * static_cast<double>(dz) + dxSquared;
                for (int32_t dy = 0; dy < height; dy++) {
                    double distance = 0.0;
                    double threshold = -0.5;
                    if (!ellipse) {
                        const int32_t radius = roundRadius(dy, static_cast<float>(dy));
                        if (dx >= radius)
                            continue;

                        distance = circleDistance(distanceSquared, radius);
                        if (distance >= 0.0)
                            continue;
                        threshold = -6.0f - static_cast<float>(nextInt(random, 3));
                    } else {
                        distance = ellipseDistance(static_cast<float>(dx), static_cast<float>(dz),
                                                   static_cast<float>(width), static_cast<float>(ellipseDepth),
                                                   cosine, sine);
                        if (distance >= -7.006492321624085e-46)
                            continue;
                    }

                    if (distance > threshold && static_cast<double>(nextFloat(random)) > 0.9)
                        continue;
                    setIcebergBlock(x + dx, SEA_LEVEL + dy, z + dz, height - dy, height);
                }
            }
        }
    }

    // floating pieces dropped, thin ones removed
    const int32_t smoothRadius = ellipse ? ellipseWidth : radiusValue / 2;
    if (smoothRadius >= 0 && height >= 0) {
        for (int32_t dx = -smoothRadius; dx <= smoothRadius; dx++) {
            for (int32_t dz = -smoothRadius; dz <= smoothRadius; dz++) {
                for (int32_t blockY = SEA_LEVEL; blockY <= SEA_LEVEL + height; blockY++) {
                    const int32_t blockX = x + dx;
                    const int32_t blockZ = z + dz;
                    const BlockState &state = blocks.getBlockAt(blockX, blockY, blockZ);
                    if (!isIceberg(state) && !SNOW_LAYER.contains(state))
                        continue;

                    if (DecorationSupport::isAir(blocks.getBlockAt(blockX, blockY - 1, blockZ))) {
                        blocks.setBlockStateAt(blockX, blockY, blockZ, air);
                        blocks.setBlockStateAt(blockX, blockY + 1, blockZ, air);
                    } else if (isIceberg(state)) {
                        const int32_t open = !isIceberg(blocks.getBlockAt(blockX, blockY, blockZ + 1))
                                             + !isIceberg(blocks.getBlockAt(blockX - 1, blockY, blockZ))
                                             + !isIceberg(blocks.getBlockAt(blockX, blockY, blockZ - 1))
                                             + !isIceberg(blocks.getBlockAt(blockX + 1, blockY, blockZ));
                        if (open >= 3)
                            blocks.setBlockStateAt(blockX, blockY, blockZ, air);
                    }
                }
            }
        }
    }

    // under the sea
    const int32_t depthSum = height + nextInt(random, 11);
    const int32_t depth = std::min(depthSum, 18);
    if (width > 0 && depthSum >= 2) {
        const float depthF = static_cast<float>(depth);
        for (int32_t dx = -width; dx < width; dx++) {
            const double dxSquared = static_cast<double>(dx) * static_cast<double>(dx);
            for (int32_t dz = -width; dz < width; dz++) {
                const double distanceSquared = static_cast<double>(dz) * static_cast<double>(dz) + dxSquared;
                for (int32_t below = 1; -below > -depth; below++) {
                    int32_t ringWidth = width;
                    if (ellipse) {
                        const auto dySquared = static_cast<double>(below) * static_cast<double>(below);
                        ringWidth = ceilOf(static_cast<float>(
                                (1.0 - dySquared / static_cast<double>(8.0f * depthF)) * static_cast<double>(width)));
                    }

                    const int32_t radius = steepRadius(below, depthF);
                    if (dx >= radius)
                        continue;

                    double distance = 0.0;
                    double threshold = -0.5;
                    if (!ellipse) {
                        distance = circleDistance(distanceSquared, radius);
                        if (distance >= 0.0)
                            continue;
                        threshold = -6.0f - static_cast<float>(nextInt(random, 3));
                    } else {
                        distance = ellipseDistance(static_cast<float>(dx), static_cast<float>(dz),
                                                   static_cast<float>(ringWidth), static_cast<float>(ellipseDepth),
                                                   cosine, sine);
                        if (distance >= -7.006492321624085e-46)
                            continue;
                    }

                    if (distance <= threshold || static_cast<double>(nextFloat(random)) <= 0.9)
                        setIcebergBlock(x + dx, SEA_LEVEL - below, z + dz, depth + below, depth);
                }
            }
        }
    }

    const float cutDraw = nextFloat(random);
    const bool cut = ellipse ? !(cutDraw <= 0.1f) : !(nextFloat(random) <= 0.7f);
    if (!cut)
        return;

    const int32_t half = radiusValue / 2;
    const int32_t bound = std::max(3, half) - 2;
    int32_t offsetX = nextInt(random, bound);
    if (nextBoolean(random))
        offsetX = half - nextInt(random, std::max(1, radiusValue - half - 1)) + 1;
    int32_t offsetZ = nextInt(random, bound);
    if (nextBoolean(random))
        offsetZ = half - nextInt(random, std::max(1, radiusValue - half - 1)) + 1;
    if (ellipse) {
        offsetX = nextInt(random, std::max(1, 6 - widthDraw));
        offsetZ = offsetX;
    }
    const bool negativeX = nextBoolean(random);
    const bool negativeZ = nextBoolean(random);
    const int32_t centreX = negativeX ? -offsetX : offsetX;
    const int32_t centreZ = negativeZ ? -offsetZ : offsetZ;
    float cutAngle = angle + 1.5707964f;
    if (!ellipse) {
        const float cutAngleDraw = nextFloat(random);
        cutAngle = (cutAngleDraw + cutAngleDraw) * PI;
    }
    const float cutCos = FeatureMath::sineAt(cutAngle * 10430.378f + 16384.0f);
    const float cutSin = FeatureMath::sineAt(cutAngle * 10430.378f);

    // sub_14D217330: an ellipse around the cut's centre emptied, to air above the sea and water under it
    const auto carve = [&](int32_t radius, int32_t dy, bool underwater) {
        const int32_t extent = radius + ellipseWidth / 3 + 1;
        if (extent <= 0)
            return;

        const auto a = static_cast<float>(extent);
        const auto b = static_cast<float>(std::min(radius, 6) + ellipseDepth / 2 - 4);
        for (int32_t dx = -extent; dx < extent; dx++) {
            for (int32_t dz = -extent; dz < extent; dz++) {
                if (ellipseDistance(static_cast<float>(dx - centreX), static_cast<float>(dz - centreZ), a, b, cutCos,
                                    cutSin) >= -7.006492321624085e-46)
                    continue;

                const int32_t blockX = x + dx;
                const int32_t blockY = SEA_LEVEL + dy;
                const int32_t blockZ = z + dz;
                const BlockState &state = blocks.getBlockAt(blockX, blockY, blockZ);
                if (!isIceberg(state) && !SNOW_LAYER.contains(state))
                    continue;

                if (underwater) {
                    blocks.setBlockStateAt(blockX, blockY, blockZ, water);
                    continue;
                }
                blocks.setBlockStateAt(blockX, blockY, blockZ, air);
                if (SNOW_LAYER.contains(blocks.getBlockAt(blockX, blockY + 1, blockZ)))
                    blocks.setBlockStateAt(blockX, blockY + 1, blockZ, air);
            }
        }
    };

    for (int32_t dy = 0; dy < height - 3; dy++)
        carve(roundRadius(dy, static_cast<float>(dy)), dy, false);

    if (nextInt(random, 5) - height <= -2) {
        int32_t dy = -1;
        int32_t below = 1;
        do {
            carve(steepRadius(below, heightF), dy, true);
            dy--;
            below++;
        } while (dy > nextInt(random, 5) - height);
    }
}

bool OverworldRulePlacer::_placeBlueIce(BlockManager &blocks, int32_t x, int32_t y, int32_t z,
                                        std::mt19937 &random) {
    // the game's blue ice feature (sub_147174460): under the sea, in or over water and next to packed ice, then 200
    // tries around it that keep one to four blue ice neighbours
    static const BlockState blueIce = VanillaBlocks::BLUE_ICE().toBlockState();
    if (y >= 63)
        return false;
    static const BlockStateSet STILL_WATER = {VanillaBlocks::WATER().getIdentifier()};
    static const BlockStateSet PACKED_ICE = {VanillaBlocks::PACKED_ICE().getIdentifier()};
    static const BlockStateSet BLUE_ICE = {VanillaBlocks::BLUE_ICE().getIdentifier()};
    static const BlockStateSet TAKES_BLUE_ICE = {
            "minecraft:air", "minecraft:water", "minecraft:packed_ice", "minecraft:ice"
    };
    if (!STILL_WATER.contains(blocks.getBlockAt(x, y, z)) && !STILL_WATER.contains(blocks.getBlockAt(x, y - 1, z)))
        return false;

    const auto isPacked = [&](int32_t blockX, int32_t blockY, int32_t blockZ) {
        return PACKED_ICE.contains(blocks.getBlockAt(blockX, blockY, blockZ));
    };
    if (!isPacked(x, y + 1, z) && !isPacked(x, y, z - 1) && !isPacked(x, y, z + 1) && !isPacked(x - 1, y, z)
        && !isPacked(x + 1, y, z))
        return false;

    blocks.setBlockStateAt(x, y, z, blueIce);
    for (int32_t attempt = 0; attempt < 200; attempt++) {
        const int32_t dy = nextInt(random, 5) - nextInt(random, 6);
        int32_t bound = 3;
        if (dy <= 1) {
            if (dy < -5)
                continue;
            bound = dy / 2 + 3;
        }

        const int32_t first = nextInt(random, bound);
        const int32_t blockX = x + first - nextInt(random, bound);
        const int32_t third = nextInt(random, bound);
        const int32_t blockZ = z + third - nextInt(random, bound);
        const int32_t blockY = y + dy;
        if (!TAKES_BLUE_ICE.contains(blocks.getBlockAt(blockX, blockY, blockZ)))
            continue;

        int32_t touching = 0;
        for (const int32_t *step: FACE_STEPS) {
            touching += BLUE_ICE.contains(blocks.getBlockAt(blockX + step[0], blockY + step[1], blockZ + step[2]));
        }
        if (touching >= 1 && touching <= 4)
            blocks.setBlockStateAt(blockX, blockY, blockZ, blueIce);
    }
    return true;
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

void OverworldRulePlacer::_placeForestRocks(BlockManager &blocks, int32_t x, int32_t z, std::mt19937 &random) {
    // the legacy forest rock feature (vtable 0x14F4A14F0, place sub_14D21D7B0): nextInt(3) rocks, each at a column of the
    // chunk, z drawn first, from its heightmap; the rule's y is not read
    for (int32_t rock = nextInt(random, 3); rock > 0; rock--) {
        const int32_t rockZ = z + nextInt(random, 16);
        const int32_t rockX = x + nextInt(random, 16);
        _placeForestRock(blocks, rockX, heightmapAt(blocks.getLevel(), rockX, rockZ), rockZ, random);
    }
}

void OverworldRulePlacer::_placeForestRock(BlockManager &blocks, int32_t x, int32_t y, int32_t z,
                                           std::mt19937 &random) {
    // the game's forest_rock_feature (vtable 0x14F2F01A0, sub_147173640) with a start radius of 0: down to a block
    // of its list, then three blobs of mossy cobblestone, each one step aside and maybe one lower than the last
    static const BlockStateSet SUPPORT = {
            VanillaBlocks::GRASS_BLOCK().getIdentifier(), VanillaBlocks::STONE().getIdentifier(),
            VanillaBlocks::DIRT().getIdentifier(), VanillaBlocks::COARSE_DIRT().getIdentifier()
    };
    int32_t rockY = y;
    while (rockY > LevelChunk::MIN_Y + 3 && !SUPPORT.contains(blocks.getBlockAt(x, rockY - 1, z)))
        rockY--;
    if (rockY <= LevelChunk::MIN_Y + 3)
        return;

    static const BlockState mossy = VanillaBlocks::MOSSY_COBBLESTONE().toBlockState();
    int32_t centreX = x;
    int32_t centreY = rockY;
    int32_t centreZ = z;
    for (int32_t blob = 0; blob < 3; blob++) {
        const int32_t radiusX = nextInt(random, 2);
        const int32_t radiusY = nextInt(random, 2);
        const int32_t radiusZ = nextInt(random, 2);
        const float reach = static_cast<float>(radiusZ + radiusY + radiusX) * 0.333f + 0.5f;
        for (int32_t offsetX = -radiusX; offsetX <= radiusX; offsetX++) {
            for (int32_t offsetY = -radiusY; offsetY <= radiusY; offsetY++) {
                for (int32_t offsetZ = -radiusZ; offsetZ <= radiusZ; offsetZ++) {
                    const auto distance = static_cast<float>(offsetX * offsetX + offsetY * offsetY + offsetZ * offsetZ);
                    if (reach * reach >= distance)
                        blocks.setBlockStateAt(centreX + offsetX, centreY + offsetY, centreZ + offsetZ, mossy);
                }
            }
        }

        const int32_t stepX = nextInt(random, 2);
        const int32_t stepY = nextInt(random, 2);
        const int32_t stepZ = nextInt(random, 2);
        centreX += stepX - 1;
        centreY -= stepY;
        centreZ += stepZ - 1;
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
        if (!DecorationSupport::isAir(blocks.getBlockAt(plantX, plantY, plantZ))
            || !DecorationSupport::isAir(blocks.getBlockAt(plantX, plantY + 1, plantZ))
            || !IFeature::isSupportDirt(blocks.getBlockAt(plantX, plantY - 1, plantZ)))
            continue;

        blocks.setBlockStateAt(plantX, plantY, plantZ, lower);
        blocks.setBlockStateAt(plantX, plantY + 1, plantZ, upper);
    }
}
