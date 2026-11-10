#include "World/Level/LevelGen/Overworld/Feature/OverworldRulePlacer.h"

#include "World/Level/LevelGen/Nether/Feature/NetherFeatures.h"
#include "World/Level/Block/Actor/BeehiveBlockActor.h"

#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/DoublePlantBlock.h"
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
#include "World/Level/LevelGen/Overworld/Feature/Decoration/BlockPileFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/CactusFeature.h"
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
#include <utility>
#include <vector>

namespace {
    constexpr float PI = 3.1415927f;
    constexpr const char *FIRST_PASS = "first_pass";
    constexpr const char *UNDERGROUND_PASS = "underground_pass";
    constexpr const char *SURFACE_PASS = "surface_pass";
    constexpr const char *AFTER_SURFACE_PASS = "after_surface_pass";

    // the passes the game places after the carvers, in order (sub_1471CE3F0)
    const char *const PASSES[] = {FIRST_PASS,
                                  "before_underground_pass",
                                  UNDERGROUND_PASS,
                                  "after_underground_pass",
                                  "before_surface_pass",
                                  SURFACE_PASS,
                                  "after_surface_pass",
                                  "before_sky_pass",
                                  "sky_pass",
                                  "after_sky_pass",
                                  "final_pass"};

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
        // step_size and grid_offset of a fixed_grid
        int32_t mStep = 0;
        int32_t mGridOffset = 0;
    };

    const int32_t FIXED_GRID = 4;

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

    // query.heightmap (sub_145E2C6D0): the game computes it once the terrain is carved, never while features
    // are placed; the generator keeps the highest non-air block there
    int32_t heightmapAt(Level &level, int32_t x, int32_t z) {
        LevelChunk *chunk = level.peekChunkPtr(x >> 4, z >> 4);
        if (chunk == nullptr)
            return LevelChunk::MIN_Y;

        return chunk->getHeight(x & 15, z & 15) + 1;
    }
    // axis visiting order of each coordinate_eval_order (sub_1437FD990)
    const int32_t EVAL_ORDERS[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};

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

    // the render params of sub_14CDD4880: the chunk's decoration random, math.random's nextFloat source
    // (sub_1437FBE80, sub_140AE3310), and the preliminary surface levels (sub_145C321F0)
    struct PlacementContext {
        std::mt19937 *mDecoration = nullptr;
        const OverworldClimate *mClimate = nullptr;
        // the second value of Random::nextGaussian, kept for its next call
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
    template <typename Continues, typename Ends>
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

    // sub_143815010; a fixed_grid reads the iteration's grid index and leaves the rest for the next axis
    // (case 4 of sub_1437FD570): a signed remainder over the extent, not offset by its minimum
    int32_t sampleAxis(const Axis &axis, const ScatterVariables &variables, std::mt19937 &random, uint32_t *grid) {
        const auto min = static_cast<int32_t>(std::round(axis.mMin->evaluate(variables)));
        if (axis.mDistribution == 0)
            return min;

        const auto max = static_cast<int32_t>(std::round(axis.mMax->evaluate(variables)));
        if (max <= min)
            return min;
        if (axis.mDistribution != FIXED_GRID || grid == nullptr)
            return distribute(axis.mDistribution, min, max, random);

        const int32_t span = max - min + 1;
        const int32_t stepped = axis.mStep * static_cast<int32_t>(*grid);
        const int32_t value = (stepped + axis.mGridOffset + min) % span;
        *grid = static_cast<uint32_t>(axis.mGridOffset + stepped) / static_cast<uint32_t>(span);
        return value;
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
                         std::mt19937 &random, int32_t position[3], uint32_t *grid = nullptr) {
        for (int32_t axis: EVAL_ORDERS[evalOrder]) {
            position[axis] = origin[axis] + sampleAxis(axes[axis], variables, random, grid);
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
        static const BlockStateSet KELP = BlockStateSet::ofBlocks(
                [](const Block &block) { return dynamic_cast<const KelpBlock *>(&block) != nullptr; });
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
        static const BlockStateSet REFUSED = {"minecraft:air", "minecraft:bedrock", "minecraft:packed_ice",
                                              "minecraft:blue_ice"};
        return REFUSED.contains(state) || isWater(state) || LavaBlock::matches(state.mName)
               || IceBlock::matches(state.mName);
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

    struct DripstoneBlocks {
        Block (*mBase)();
        Block (*mPointed)();
        BlockStateSet mReplaceable;
    };

    // xmmword_1529AA840
    const DripstoneBlocks &dripstoneBlocks() {
        static const DripstoneBlocks blocks = {&VanillaBlocks::DRIPSTONE_BLOCK,
                                               &VanillaBlocks::POINTED_DRIPSTONE,
                                               {"minecraft:dirt", "minecraft:coarse_dirt", "minecraft:stone",
                                                "minecraft:tuff", "minecraft:deepslate"}};
        return blocks;
    }

    // xmmword_1529AA888, registered in sub_14726F340
    const DripstoneBlocks &sulfurSpikeBlocks() {
        static const DripstoneBlocks blocks = {
                &VanillaBlocks::SULFUR, &VanillaBlocks::SULFUR_SPIKE, {"minecraft:sulfur", "minecraft:cinnabar"}};
        return blocks;
    }

    bool isDripstoneBase(const DripstoneBlocks &dripstone, const BlockState &state) {
        return state.mName == dripstone.mBase().getIdentifier();
    }

    bool isPointedDripstone(const DripstoneBlocks &dripstone, const BlockState &state) {
        return state.mName == dripstone.mPointed().getIdentifier();
    }

    // sub_14721CC10
    bool isDripstoneBaseOrLava(const DripstoneBlocks &dripstone, BlockManager &blocks, int32_t x, int32_t y,
                               int32_t z) {
        const BlockState &state = blocks.getBlockAt(x, y, z);
        return dripstone.mReplaceable.contains(state) || isDripstoneBase(dripstone, state) || isLava(blocks, x, y, z);
    }

    // sub_1471E6440
    bool placeDripstoneBlock(const DripstoneBlocks &dripstone, BlockManager &blocks, int32_t x, int32_t y, int32_t z) {
        if (!dripstone.mReplaceable.contains(blocks.getBlockAt(x, y, z)))
            return false;

        blocks.setBlockStateAt(x, y, z, dripstone.mBase().toBlockState());
        return true;
    }

    // sub_1471E6570: from its base, base, middle, frustum and the tip or merge, as long as the blocks are open
    void growPointedDripstone(const DripstoneBlocks &dripstone, BlockManager &blocks, int32_t x, int32_t y, int32_t z,
                              bool up, int32_t length, bool merge) {
        if (length <= 0)
            return;

        // sub_14721CE00: thickness 0 tip, 1 frustum, 2 middle, 3 base, 4 merge
        static const char *const THICKNESS[5] = {"tip", "frustum", "middle", "base", "merge"};
        const BlockState pointed = dripstone.mPointed().toBlockState();
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

namespace {
    // VinesFeature::place (vtable 0x14F2F6FC0, sub_14721B910): up the column from y 64, every air block gets a vine
    // on the first of north, south, west and east whose support holds; a block in the way moves the column by two
    // raw draws per axis from the origin; the game stops at the highest block of the chunks around, the top here
    bool placeVines(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random) {
        (void) y;
        // face, support offset and vine_direction_bits (1 << qword_14F0F3EF0[qword_14F0AE1E0[face]])
        static const int32_t FACES[4][3] = {{0, 1, 1}, {0, -1, 4}, {1, 0, 8}, {-1, 0, 2}};
        static const BlockState vine = VanillaBlocks::VINE().toBlockState();
        int32_t columnX = x;
        int32_t columnZ = z;
        for (int32_t columnY = 64; columnY <= LevelChunk::MAX_Y; columnY++) {
            if (!DecorationSupport::isAir(blocks.getBlockAt(columnX, columnY, columnZ))) {
                const int32_t awayX = static_cast<int32_t>(random() & 3);
                columnX = x + awayX - static_cast<int32_t>(random() & 3);
                const int32_t awayZ = static_cast<int32_t>(random() & 3);
                columnZ = z + awayZ - static_cast<int32_t>(random() & 3);
                continue;
            }

            for (const int32_t *face: FACES) {
                if (!BlockSupport::isSolid(blocks.getBlockAt(columnX + face[0], columnY, columnZ + face[1])))
                    continue;

                Tag states = vine.mStates;
                states.putInt("vine_direction_bits", face[2]);
                blocks.setBlockStateAt(columnX, columnY, columnZ, BlockState(vine.mName, states));
                break;
            }
        }
        return true;
    }

    // MelonFeature::place (vtable 0x14F2F6F90, sub_14D224EF0): 64 tries around the position, z, y then x each the
    // difference of two raw draws, a melon where it may stand on the dirt material
    bool placeMelon(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random) {
        static const BlockState melon = VanillaBlocks::MELON_BLOCK().toBlockState();
        for (int32_t attempt = 0; attempt < 64; attempt++) {
            const auto awayZ = static_cast<int32_t>(random() & 7);
            const int32_t melonZ = z + awayZ - static_cast<int32_t>(random() & 7);
            const auto awayY = static_cast<int32_t>(random() & 3);
            const int32_t melonY = y + awayY - static_cast<int32_t>(random() & 3);
            const auto awayX = static_cast<int32_t>(random() & 7);
            const int32_t melonX = x + awayX - static_cast<int32_t>(random() & 7);
            if (BlockSupport::isReplaceable(blocks.getBlockAt(melonX, melonY, melonZ))
                && IFeature::isSupportDirt(blocks.getBlockAt(melonX, melonY - 1, melonZ)))
                blocks.setBlockStateAt(melonX, melonY, melonZ, melon);
        }
        return true;
    }

    // HorizontalTreeDecorationFeature::place (sub_14D2136C0, vtable 0x14F2F67C0) of shelf_mushroom_feature:
    // bark_side_only, allow_adjacent false; a side of the log by a draw, the growth by a second once it fits
    bool placeShelfMushroom(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random) {
        static const int32_t SIDES[4][3] = {{0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};
        static const char *const CARDINALS[4] = {"north", "south", "west", "east"};
        const std::string shelf = VanillaBlocks::SHELF_MUSHROOM().getIdentifier();
        const int32_t side = nextInt(random, 4);
        const int32_t toX = x + SIDES[side][0];
        const int32_t toZ = z + SIDES[side][2];
        if (!DecorationSupport::isAir(blocks.getBlockAt(toX, y, toZ)))
            return false;

        const auto isShelf = [&](int32_t atX, int32_t atY, int32_t atZ) {
            return blocks.getBlockAt(atX, atY, atZ).mName == shelf;
        };
        if (isShelf(x, y, z - 1) || isShelf(x + 1, y, z) || isShelf(x, y, z + 1) || isShelf(x - 1, y, z)
            || isShelf(toX, y + 1, toZ) || isShelf(toX, y - 1, toZ) || isShelf(toX, y, toZ - 1)
            || isShelf(toX, y, toZ + 1) || isShelf(toX - 1, y, toZ) || isShelf(toX + 1, y, toZ))
            return false;

        // never on the ends of a lying log
        const std::string axis = blocks.getBlockAt(x, y, z).mStates.getString("pillar_axis", "y");
        if ((side >= 2 && axis == "x") || (side < 2 && axis == "z"))
            return false;

        Tag states = VanillaBlocks::SHELF_MUSHROOM().toBlockState().mStates;
        states.putInt("growth", nextInt(random, 2));
        states.putString("minecraft:cardinal_direction", CARDINALS[side]);
        blocks.setBlockStateAt(toX, y, toZ, BlockState(shelf, states));
        return true;
    }

    // BambooFeature::place (vtable 0x14F2F70E0, sub_147171FC0): thick stalks up from an air block over dirt, sand or
    // gravel, 5 plus nextInt of 8 to 12 by a hash of the column; from 3 tall one more block on top, large leaves on
    // the two highest and small ones under them
    bool placeBamboo(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random) {
        static const BlockStateSet GROUND = BlockStateSet::ofBlocks([](const Block &block) {
            const std::string &name = block.getIdentifier();
            return IFeature::isSupportDirt(block.toBlockState()) || name == VanillaBlocks::SAND().getIdentifier()
                   || name == VanillaBlocks::RED_SAND().getIdentifier()
                   || name == VanillaBlocks::GRAVEL().getIdentifier();
        });
        if (!DecorationSupport::isAir(blocks.getBlockAt(x, y, z)) || !GROUND.contains(blocks.getBlockAt(x, y - 1, z)))
            return false;

        const BlockState stalk = withState(VanillaBlocks::BAMBOO().toBlockState(), "bamboo_stalk_thickness", "thick");
        const uint32_t mixed = (3129871u * static_cast<uint32_t>(x)) ^ (116129781u * static_cast<uint32_t>(z));
        const uint32_t hash = (mixed * (42317861u * mixed + 11u)) >> 24;
        const int32_t extra = nextInt(random, static_cast<int32_t>((hash % 5u) | 8u));
        int32_t top = y;
        for (int32_t count = extra + 5; count > 0 && DecorationSupport::isAir(blocks.getBlockAt(x, top, z)); count--)
            blocks.setBlockStateAt(x, top++, z, stalk);
        if (top - y < 3)
            return true;

        const BlockState large = withState(stalk, "bamboo_leaf_size", "large_leaves");
        blocks.setBlockStateAt(x, top, z, large);
        blocks.setBlockStateAt(x, top - 1, z, large);
        blocks.setBlockStateAt(x, top - 2, z, withState(stalk, "bamboo_leaf_size", "small_leaves"));
        return true;
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

// DripstoneClusterFeature (sub_1471E6910) or PointedDripstoneFeature (sub_1471E82A0); a pointed one reads the blocks only
struct OverworldRulePlacer::DripstoneDefinition {
    const DripstoneBlocks &(*mBlocks)();
    int32_t mMinHeight;
    int32_t mMaxHeight;
    // clamped normal: mean, deviation, low, high
    float mWetness[4];
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
        int32_t mStep = 0;
        int32_t mGridOffset = 0;
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
        for (size_t axis = 0; axis < 3; axis++) {
            // the rules never use fixed_grid: only the scatter features parse it
            const bool grid = texts[axis].mDistribution == FIXED_GRID;
            parseAxis(grid ? 1 : texts[axis].mDistribution, texts[axis].mMin, texts[axis].mMax, axes[axis]);
            if (!grid)
                continue;
            axes[axis].mDistribution = FIXED_GRID;
            axes[axis].mStep = texts[axis].mStep;
            axes[axis].mGridOffset = texts[axis].mGridOffset;
        }
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
    // may_attach_to with a side of this block among the four around, auto_rotate turning it to face that side;
    // without may_replace the block goes over anything
    Block (*mAttachesTo)() = nullptr;
    int32_t mAge = 0;
    // may_not_attach_to bottom podzol, tested before the weight is drawn
    bool mNotOnPodzol = false;
    // survives on sand and hardened clay besides the blocks of PlantSupport::Dirt
    bool mSandAndClay = false;
};

// a minecraft:sequence_feature: each feature at the position the one before hands on
struct OverworldRulePlacer::SequenceDefinition {
    const char *mIdentifier;
    std::vector<Placed> mFeatures;
};

// a minecraft:weighted_random_feature
struct OverworldRulePlacer::WeightedDefinition {
    const char *mIdentifier;
    std::vector<std::pair<Placed, int32_t>> mFeatures;
};

// a minecraft:aggregate_feature (place sub_14A2DDCA0): every feature at the same position; early_out 1 stops
// while none has been placed, 2 at the first placed
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
    const OverworldTreePlacer::Acacia *mAcacia = nullptr;
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
        // nextInt(3) != 0 ? (nextInt(3) != 0 ? third : second) : first, sub_14D230920
        RedwoodMutated,
        // nextInt(5) != 0 && nextBoolean() ? second : first, sub_14D2302A0
        BirchMutated,
        // nextInt(10) != 0 ? (nextInt(2) != 0 ? (nextInt(3) != 0 ? fourth : third) : second) : first, sub_14D230560
        Jungle,
        // nextInt(10) != 0 ? (nextInt(2) != 0 ? third : second) : first, sub_14D230630
        JungleEdge,
        // nextInt(20) != 0 ? (nextInt(20) > 2 ? (nextInt(10) > 6 ? fourth : third) : second) : first, sub_14D230180
        BambooJungle,
        // nextInt(5) > 0 ? first : second, sub_14D230780
        Savanna
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

// a minecraft:vegetation_patch_feature (place sub_14A282710): a disk snapped to the floor or ceiling, the ground
// laid first, then a vegetation feature on a share of its columns
struct OverworldRulePlacer::VegetationPatchDefinition {
    const char *mIdentifier;
    Block (*mGround)();
    std::vector<std::string> mReplaceable;
    int32_t mDepthMin;
    int32_t mDepthMax;
    int32_t mVerticalRange;
    float mExtraDeepChance;
    float mExtraEdgeChance;
    int32_t mRadiusMin;
    int32_t mRadiusMax;
    // floor grows the disk downward, ceiling upward
    bool mFloor;
    // the vegetation on a share of the ground: weighted blocks, drawn even when empty to keep the shared
    // placement random in step across the rule's iterations
    std::vector<std::pair<Block (*)(), int32_t>> mVegetation;
    // a feature placed instead of the weighted blocks: the moss ceiling's cave vines, the clay's dripleaves
    Placed mVegetationFeature;
    float mVegetationChance;
    // the surface turns to water where closed on its four sides and below; the vegetation goes in that water,
    // not above it
    bool mWaterlogged;
};

// a minecraft:growing_plant_feature (place sub_14A2BEF40): a hanging or standing column of weighted body blocks
// capped with a weighted head, its length drawn from a weighted set of ranges
struct OverworldRulePlacer::GrowingPlantDefinition {
    struct HeightWeight {
        int32_t mMin;
        int32_t mMax;
        int32_t mWeight;
    };
    struct BlockWeight {
        BlockState mState;
        int32_t mWeight;
    };

    const char *mIdentifier;
    std::vector<HeightWeight> mHeight;
    // down grows toward lower y
    bool mDown;
    // an age range ending at 0 is no age: the head keeps its own state and nothing is drawn
    int32_t mAgeMin;
    int32_t mAgeMax;
    std::vector<BlockWeight> mBody;
    std::vector<BlockWeight> mHead;
    // allow_water: the plant also grows through water, the next step still only through air
    bool mAllowWater;
};

OverworldRulePlacer::Feature OverworldRulePlacer::_find(const BiomeFeatureEntry &entry) {
    // BDS 1.26.61 behavior packs, features/*.json
    static const std::vector<OreDefinition> ORES = {
            {"minecraft:andesite_feature", 64, 0.0f, {{&VanillaBlocks::ANDESITE, {"minecraft:stone"}}}},
            {"minecraft:clay_ore_feature", 33, 0.0f, {{&VanillaBlocks::CLAY, {"minecraft:stone"}}}},
            {"minecraft:coal_ore_feature",
             17,
             0.5f,
             {{&VanillaBlocks::COAL_ORE, {"minecraft:stone"}},
              {&VanillaBlocks::DEEPSLATE_COAL_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:coal_ore_upper_feature",
             17,
             0.0f,
             {{&VanillaBlocks::COAL_ORE, {"minecraft:stone"}},
              {&VanillaBlocks::DEEPSLATE_COAL_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:copper_ore_feature",
             10,
             0.0f,
             {{&VanillaBlocks::COPPER_ORE, {"minecraft:stone"}},
              {&VanillaBlocks::DEEPSLATE_COPPER_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:diamond_ore_buried_feature",
             8,
             1.0f,
             {{&VanillaBlocks::DIAMOND_ORE, {"minecraft:stone"}},
              {&VanillaBlocks::DEEPSLATE_DIAMOND_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:diamond_ore_feature",
             4,
             0.5f,
             {{&VanillaBlocks::DIAMOND_ORE, {"minecraft:stone"}},
              {&VanillaBlocks::DEEPSLATE_DIAMOND_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:diamond_ore_large_feature",
             12,
             0.7f,
             {{&VanillaBlocks::DIAMOND_ORE, {"minecraft:stone"}},
              {&VanillaBlocks::DEEPSLATE_DIAMOND_ORE, {"minecraft:deepslate"}}}},
            // not in the packs, only in the game's definitions/features/dirt_feature.json
            {"minecraft:dirt_feature", 33, 0.0f, {{&VanillaBlocks::DIRT, {"minecraft:stone"}}}},
            {"minecraft:diorite_feature", 64, 0.0f, {{&VanillaBlocks::DIORITE, {"minecraft:stone"}}}},
            {"minecraft:dripstone_caves_copper_ore_feature",
             20,
             0.0f,
             {{&VanillaBlocks::COPPER_ORE, {"minecraft:stone"}},
              {&VanillaBlocks::DEEPSLATE_COPPER_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:emerald_ore_feature",
             3,
             0.0f,
             {{&VanillaBlocks::EMERALD_ORE, {"minecraft:stone"}},
              {&VanillaBlocks::DEEPSLATE_EMERALD_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:gold_ore_extra_feature",
             9,
             0.0f,
             {{&VanillaBlocks::GOLD_ORE, {"minecraft:stone"}},
              {&VanillaBlocks::DEEPSLATE_GOLD_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:gold_ore_feature",
             9,
             0.5f,
             {{&VanillaBlocks::GOLD_ORE, {"minecraft:stone"}},
              {&VanillaBlocks::DEEPSLATE_GOLD_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:gold_ore_lower_feature",
             9,
             0.5f,
             {{&VanillaBlocks::GOLD_ORE, {"minecraft:stone"}},
              {&VanillaBlocks::DEEPSLATE_GOLD_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:granite_feature", 64, 0.0f, {{&VanillaBlocks::GRANITE, {"minecraft:stone"}}}},
            {"minecraft:gravel_ore_feature",
             33,
             0.0f,
             {{&VanillaBlocks::GRAVEL, {"minecraft:stone", "minecraft:deepslate"}}}},
            {"minecraft:iron_ore_feature",
             10,
             0.0f,
             {{&VanillaBlocks::IRON_ORE, {"minecraft:stone"}},
              {&VanillaBlocks::DEEPSLATE_IRON_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:iron_ore_small_feature",
             4,
             0.0f,
             {{&VanillaBlocks::IRON_ORE, {"minecraft:stone"}},
              {&VanillaBlocks::DEEPSLATE_IRON_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:lapis_ore_buried_feature",
             7,
             1.0f,
             {{&VanillaBlocks::LAPIS_ORE, {"minecraft:stone"}},
              {&VanillaBlocks::DEEPSLATE_LAPIS_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:lapis_ore_feature",
             7,
             0.0f,
             {{&VanillaBlocks::LAPIS_ORE, {"minecraft:stone"}},
              {&VanillaBlocks::DEEPSLATE_LAPIS_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:redstone_ore_feature",
             8,
             0.0f,
             {{&VanillaBlocks::REDSTONE_ORE, {"minecraft:stone"}},
              {&VanillaBlocks::DEEPSLATE_REDSTONE_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:tuff_feature", 64, 0.0f, {{&VanillaBlocks::TUFF, {"minecraft:stone", "minecraft:deepslate"}}}},
            {"minecraft:nether_soul_sand_underground_feature",
             12,
             0.0f,
             {{&VanillaBlocks::SOUL_SAND, {"minecraft:netherrack"}}}}};

    // vtables 0x14F4A0DC0 (clay, place sub_14D1FBFA0) and 0x14F4A1710 (sand and gravel, place sub_14D22F1B0)
    static const DiskDefinition DISKS[] = {{"minecraft:sand_feature",
                                            &VanillaBlocks::SAND,
                                            7,
                                            2,
                                            {"minecraft:dirt", "minecraft:coarse_dirt", "minecraft:grass_block"},
                                            false},
                                           {"minecraft:gravel_feature",
                                            &VanillaBlocks::GRAVEL,
                                            6,
                                            2,
                                            {"minecraft:dirt", "minecraft:coarse_dirt", "minecraft:grass_block"},
                                            false},
                                           {"minecraft:clay_feature",
                                            &VanillaBlocks::CLAY,
                                            4,
                                            1,
                                            {"minecraft:dirt", "minecraft:coarse_dirt", "minecraft:mud"},
                                            true}};

    static const SeagrassDefinition SEAGRASS[] = {{"minecraft:shortest_seagrass_feature", 1, 0.3f},
                                                  {"minecraft:shorter_seagrass_feature", 1, 0.4f},
                                                  {"minecraft:taller_seagrass_feature", 1, 0.8f},
                                                  {"minecraft:seagrass_feature", 1, 0.6f}};

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
            {"minecraft:nether_sprouts_feature", &VanillaBlocks::NETHER_SPROUTS, true, false, -1, false, true},
            {"minecraft:jungle_tall_grass_feature", &VanillaBlocks::SHORT_GRASS, true, false, -1, false, false, nullptr,
             0, true},
            {"minecraft:jungle_fern_feature", &VanillaBlocks::FERN, true, false, -1, false, false, nullptr, 0, true},
            // vanilla_1.26.50 features/red_shrub_single_block_feature.json
            {"minecraft:red_shrub_single_block_feature", &VanillaBlocks::RED_SHRUB, true, false, -1, false, false,
             nullptr, 0, false, true}};

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
            {"minecraft:select_taiga_plant_feature", {{single(1), 4}, {single(0), 1}}},
            {"minecraft:select_jungle_plant_feature", {{single(13), 1}, {single(12), 3}}}};

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
            {"minecraft:scatter_jungle_plant_feature", "32", {GRASS_SIDE, GRASS_HEIGHT, GRASS_SIDE}, 5, weighted(1)},
            {"minecraft:crimson_fungus_scatter_feature",
             "24",
             {{2, "-8", "8"}, {2, "-4", "4"}, {2, "-8", "8"}},
             5,
             single(7)},
            {"minecraft:warped_fungus_scatter_feature",
             "24",
             {{2, "-8", "8"}, {2, "-4", "4"}, {2, "-8", "8"}},
             5,
             single(8)},
            {"minecraft:crimson_roots_scatter_feature",
             "64",
             {{2, "-8", "8"}, {2, "-4", "4"}, {2, "-8", "8"}},
             5,
             single(9)},
            {"minecraft:warped_roots_scatter_feature",
             "64",
             {{2, "-8", "8"}, {2, "-4", "4"}, {2, "-8", "8"}},
             5,
             single(10)},
            {"minecraft:nether_sprouts_scatter_feature",
             "64",
             {{2, "-8", "8"}, {2, "-4", "4"}, {2, "-8", "8"}},
             5,
             single(11)},
            {"minecraft:scatter_cold_ocean_seagrass_feature",
             "32",
             {SEAGRASS_SIDE, SEAGRASS_HEIGHT, SEAGRASS_SIDE},
             4,
             seagrass(0)},
            {"minecraft:scatter_deep_cold_ocean_seagrass_feature",
             "40",
             {SEAGRASS_SIDE, SEAGRASS_HEIGHT, SEAGRASS_SIDE},
             4,
             seagrass(2)},
            {"minecraft:scatter_deep_ocean_seagrass_feature",
             "48",
             {SEAGRASS_SIDE, SEAGRASS_HEIGHT, SEAGRASS_SIDE},
             4,
             seagrass(2)},
            {"minecraft:scatter_deep_warm_ocean_seagrass_feature",
             "80",
             {SEAGRASS_SIDE, SEAGRASS_HEIGHT, SEAGRASS_SIDE},
             4,
             seagrass(2)},
            {"minecraft:scatter_ocean_seagrass_feature",
             "48",
             {SEAGRASS_SIDE, SEAGRASS_HEIGHT, SEAGRASS_SIDE},
             4,
             seagrass(0)},
            {"minecraft:scatter_river_seagrass_feature",
             "48",
             {SEAGRASS_SIDE, SEAGRASS_HEIGHT, SEAGRASS_SIDE},
             4,
             seagrass(1)},
            {"minecraft:scatter_swamp_seagrass_feature",
             "64",
             {SEAGRASS_SIDE, SEAGRASS_HEIGHT, SEAGRASS_SIDE},
             4,
             seagrass(3)},
            {"minecraft:scatter_warm_ocean_seagrass_feature",
             "80",
             {SEAGRASS_SIDE, SEAGRASS_HEIGHT, SEAGRASS_SIDE},
             4,
             seagrass(0)},
            {"minecraft:scatter_tall_grass_feature", "32", {GRASS_SIDE, GRASS_HEIGHT, GRASS_SIDE}, 5, single(0)},
            {"minecraft:scatter_meadow_tall_grass_feature", "16", {GRASS_SIDE, GRASS_HEIGHT, GRASS_SIDE}, 5, single(0)},
            {"minecraft:scatter_taiga_plant_feature", "32", {GRASS_SIDE, GRASS_HEIGHT, GRASS_SIDE}, 5, weighted(0)},
            {"minecraft:scatter_bush_feature", "24", {{6, "-5", "5"}, {6, "-3", "3"}, {6, "-5", "5"}}, 5, single(6)}};

    // registered in sub_14726F340
    static const LegacyFlowerDefinition LEGACY_FLOWERS[] = {{"minecraft:legacy:overworld_flower_feature", 2},
                                                            {"minecraft:legacy:plains_flower_feature", 3}};
    static const auto legacyFlower = [](size_t index) {
        Placed placed;
        placed.mLegacyFlower = &LEGACY_FLOWERS[index];
        return placed;
    };
    // BDS 1.26.61 behavior packs, features/scatter_overworld_flower_feature.json and scatter_plains_flower_feature.json
    static const ScatterDefinition FLOWER_SCATTERS[] = {{"minecraft:scatter_overworld_flower_feature",
                                                         "64",
                                                         {{6, "-7", "7"}, {6, "-3", "3"}, {6, "-7", "7"}},
                                                         5,
                                                         legacyFlower(0)},
                                                        {"minecraft:scatter_plains_flower_feature",
                                                         "64",
                                                         {{6, "-6", "6"}, {6, "-2", "2"}, {6, "-6", "6"}},
                                                         5,
                                                         legacyFlower(1)}};

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
    static const ScatterDefinition LOG_BROWN_MUSHROOM = {"minecraft:optional_log_brown_mushroom_feature",
                                                         "1",
                                                         {ZERO, ZERO, ZERO},
                                                         0,
                                                         mushroom(&VanillaBlocks::BROWN_MUSHROOM),
                                                         0,
                                                         0,
                                                         "50.0"};
    static const AggregateDefinition SELECT_LOG_MUSHROOM = {
            "minecraft:select_log_mushroom_feature",
            {scatter(LOG_BROWN_MUSHROOM), mushroom(&VanillaBlocks::RED_MUSHROOM)},
            2};
    static const ScatterDefinition LOG_MUSHROOMS = {"minecraft:optional_log_mushrooms_feature",
                                                    "1",
                                                    {ZERO, {0, "1", ""}, ZERO},
                                                    0,
                                                    aggregate(SELECT_LOG_MUSHROOM),
                                                    1,
                                                    10};

    using Tree = OverworldTreePlacer;
    static const Tree::Fancy FANCY_OAK = {5, 12, 0.618f, 1, 0.381f, 1.0f, 0.2f, 1.0f, 0.3f, 4, 3};
    static const TreeDefinition TREES[] = {{{"minecraft:oak_tree_feature",
                                             Tree::Trunk::Simple,
                                             &VanillaBlocks::OAK_LOG,
                                             {4, 7},
                                             {0, 0},
                                             Tree::Canopy::Simple,
                                             &VanillaBlocks::OAK_LEAVES,
                                             {0, 0},
                                             {0, 0},
                                             {0, 0},
                                             0,
                                             0.0f,
                                             false},
                                            nullptr,
                                            {}},
                                           {{"minecraft:oak_tree_with_vines_feature",
                                             Tree::Trunk::Simple,
                                             &VanillaBlocks::OAK_LOG,
                                             {4, 7},
                                             {0, 0},
                                             Tree::Canopy::Simple,
                                             &VanillaBlocks::OAK_LEAVES,
                                             {0, 0},
                                             {0, 0},
                                             {0, 0},
                                             0,
                                             100.0f,
                                             false},
                                            nullptr,
                                            {}},
                                           {{"minecraft:fancy_oak_tree_feature",
                                             Tree::Trunk::Fancy,
                                             &VanillaBlocks::OAK_LOG,
                                             {0, 0},
                                             {0, 0},
                                             Tree::Canopy::Fancy,
                                             &VanillaBlocks::OAK_LEAVES,
                                             {0, 0},
                                             {0, 0},
                                             {0, 0},
                                             0,
                                             0.0f,
                                             false},
                                            &FANCY_OAK,
                                            {}},
                                           {{"minecraft:fallen_oak_tree_feature",
                                             Tree::Trunk::Fallen,
                                             &VanillaBlocks::OAK_LOG,
                                             {4, 7},
                                             {0, 0},
                                             Tree::Canopy::None,
                                             nullptr,
                                             {0, 0},
                                             {0, 0},
                                             {0, 0},
                                             0,
                                             75.0f,
                                             true},
                                            nullptr,
                                            scatter(LOG_MUSHROOMS)},
                                           {{"minecraft:birch_tree_feature",
                                             Tree::Trunk::Simple,
                                             &VanillaBlocks::BIRCH_LOG,
                                             {5, 8},
                                             {0, 0},
                                             Tree::Canopy::Simple,
                                             &VanillaBlocks::BIRCH_LEAVES,
                                             {0, 0},
                                             {0, 0},
                                             {0, 0},
                                             0,
                                             0.0f,
                                             false},
                                            nullptr,
                                            {}},
                                           {{"minecraft:fallen_birch_tree_feature",
                                             Tree::Trunk::Fallen,
                                             &VanillaBlocks::BIRCH_LOG,
                                             {5, 8},
                                             {0, 0},
                                             Tree::Canopy::None,
                                             nullptr,
                                             {0, 0},
                                             {0, 0},
                                             {0, 0},
                                             0,
                                             0.0f,
                                             true},
                                            nullptr,
                                            scatter(LOG_MUSHROOMS)},
                                           {{"minecraft:spruce_tree_feature",
                                             Tree::Trunk::Simple,
                                             &VanillaBlocks::SPRUCE_LOG,
                                             {6, 10},
                                             {-2, 1},
                                             Tree::Canopy::Spruce,
                                             &VanillaBlocks::SPRUCE_LEAVES,
                                             {1, 3},
                                             {0, 3},
                                             {2, 4},
                                             0,
                                             0.0f,
                                             false},
                                            nullptr,
                                            {}},
                                           {{"minecraft:spruce_tree_with_vines_feature",
                                             Tree::Trunk::Simple,
                                             &VanillaBlocks::SPRUCE_LOG,
                                             {6, 10},
                                             {-2, 1},
                                             Tree::Canopy::Spruce,
                                             &VanillaBlocks::SPRUCE_LEAVES,
                                             {1, 3},
                                             {0, 3},
                                             {2, 4},
                                             0,
                                             100.0f,
                                             false},
                                            nullptr,
                                            {}},
                                           {{"minecraft:fallen_spruce_tree_feature",
                                             Tree::Trunk::Fallen,
                                             &VanillaBlocks::SPRUCE_LOG,
                                             {6, 10},
                                             {0, 0},
                                             Tree::Canopy::None,
                                             nullptr,
                                             {0, 0},
                                             {0, 0},
                                             {0, 0},
                                             0,
                                             0.0f,
                                             true},
                                            nullptr,
                                            scatter(LOG_MUSHROOMS)},
                                           // the pine canopy height takes the upper offset's place
                                           {{"minecraft:pine_tree_feature",
                                             Tree::Trunk::Simple,
                                             &VanillaBlocks::SPRUCE_LOG,
                                             {7, 12},
                                             {0, 0},
                                             Tree::Canopy::Pine,
                                             &VanillaBlocks::SPRUCE_LEAVES,
                                             {0, 0},
                                             {3, 5},
                                             {0, 0},
                                             1,
                                             0.0f,
                                             false},
                                            nullptr,
                                            {}}};

    // BDS 1.26.61 behavior packs, features/swamp_tree_feature.json
    static const TreeDefinition SWAMP_TREE = {{"minecraft:swamp_tree_feature",
                                               Tree::Trunk::Simple,
                                               &VanillaBlocks::OAK_LOG,
                                               {5, 9},
                                               {0, 0},
                                               Tree::Canopy::Simple,
                                               &VanillaBlocks::OAK_LEAVES,
                                               {0, 0},
                                               {0, 0},
                                               {0, 0},
                                               0,
                                               0.0f,
                                               false,
                                               2,
                                               2,
                                               1,
                                               4,
                                               4,
                                               true},
                                              nullptr,
                                              {}};
    static const Placed HUGE_MUSHROOM = [] {
        Placed placed;
        placed.mHugeMushroom = true;
        return placed;
    }();

    // BDS 1.26.61 behavior packs, features/mega_spruce_tree_feature.json and mega_pine_tree_feature.json
    static const Tree::Mega MEGA_SPRUCE = {2, 13, {3, 15}, {13, 18}, 0, 3.5f, 2, &VanillaBlocks::PODZOL, 5, 2};
    static const Tree::Mega MEGA_PINE = {2, 13, {3, 15}, {3, 8}, 0, 3.5f, 2, &VanillaBlocks::PODZOL, 5, 2};
    static const TreeDefinition MEGA_TREES[] = {{{"minecraft:mega_spruce_tree_feature",
                                                  Tree::Trunk::Mega,
                                                  &VanillaBlocks::SPRUCE_LOG,
                                                  {0, 0},
                                                  {0, 0},
                                                  Tree::Canopy::MegaPine,
                                                  &VanillaBlocks::SPRUCE_LEAVES,
                                                  {0, 0},
                                                  {0, 0},
                                                  {0, 0},
                                                  0,
                                                  0.0f,
                                                  false},
                                                 nullptr,
                                                 {},
                                                 &MEGA_SPRUCE},
                                                {{"minecraft:mega_pine_tree_feature",
                                                  Tree::Trunk::Mega,
                                                  &VanillaBlocks::SPRUCE_LOG,
                                                  {0, 0},
                                                  {0, 0},
                                                  Tree::Canopy::MegaPine,
                                                  &VanillaBlocks::SPRUCE_LEAVES,
                                                  {0, 0},
                                                  {0, 0},
                                                  {0, 0},
                                                  0,
                                                  0.0f,
                                                  false},
                                                 nullptr,
                                                 {},
                                                 &MEGA_PINE}};

    static const auto optional = [](const char *identifier, const Placed &places, int32_t denominator) {
        return ScatterDefinition{identifier, "1", {ZERO, ZERO, ZERO}, 0, places, 1, denominator};
    };
    static const ScatterDefinition OPTIONAL_TREES[] = {
            optional("minecraft:optional_fallen_oak_tree_feature", tree(TREES[3]), 80),
            optional("minecraft:optional_oak_tree_with_vines_feature", tree(TREES[1]), 12),
            optional("minecraft:optional_fallen_birch_tree_feature", tree(TREES[5]), 80),
            optional("minecraft:optional_fallen_spruce_tree_feature", tree(TREES[8]), 80),
            optional("minecraft:optional_spruce_tree_with_vines_feature", tree(TREES[7]), 12)};
    static const ScatterDefinition OPTIONAL_BEEHIVE = {
            "minecraft:optional_beehive_feature",
            "1",
            {ZERO, ZERO, ZERO},
            0,
            BEEHIVE_SEARCH,
            0,
            0,
            "query.has_biome_tag('bee_habitat') ? (query.has_biome_tag('plains') || "
            "query.has_biome_tag('sunflower_plains') ? 5.0 : (query.has_biome_tag('flower_forest') ? 3.0 : "
            "(query.has_biome_tag('birch_forest') || query.has_biome_tag('birch_forest_hills') || "
            "query.has_biome_tag('birch_forest_mutated') || query.has_biome_tag('forest') || "
            "query.has_biome_tag('forest_mutated') ? 0.035 : 0.0))) : 0.0"};
    static const ScatterDefinition LEAF_LITTER_SCATTER = {"minecraft:scatter_leaf_litter_feature",
                                                          "96",
                                                          {{2, "-4", "4"}, {2, "-2", "2"}, {2, "-4", "4"}},
                                                          4,
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
             {aggregate(SELECT_BIRCH), scatter(LEAF_LITTER_SCATTER)},
             1},
            {"minecraft:fancy_oak_tree_with_leaf_litter_and_optional_beehive",
             {aggregate(FANCY_WITH_BEEHIVE), scatter(LEAF_LITTER_SCATTER)},
             1},
            {"minecraft:select_oak_tree_with_leaf_litter_feature",
             {aggregate(SELECT_OAK), scatter(LEAF_LITTER_SCATTER)},
             1}};

    // the 1.26.61 features the game loads: the jungle trees, their cocoa and the jungle bush
    static const TreeDefinition JUNGLE_TREES[] = {{{"minecraft:jungle_tree_feature",
                                                    Tree::Trunk::Simple,
                                                    &VanillaBlocks::JUNGLE_LOG,
                                                    {4, 11},
                                                    {0, 0},
                                                    Tree::Canopy::Simple,
                                                    &VanillaBlocks::JUNGLE_LEAVES,
                                                    {0, 0},
                                                    {0, 0},
                                                    {0, 0},
                                                    0,
                                                    33.33f,
                                                    false,
                                                    0,
                                                    1,
                                                    1,
                                                    4,
                                                    4},
                                                   nullptr,
                                                   {}},
                                                  {{"minecraft:fallen_jungle_tree_feature",
                                                    Tree::Trunk::Fallen,
                                                    &VanillaBlocks::JUNGLE_LOG,
                                                    {4, 11},
                                                    {0, 0},
                                                    Tree::Canopy::None,
                                                    nullptr,
                                                    {0, 0},
                                                    {0, 0},
                                                    {0, 0},
                                                    0,
                                                    75.0f,
                                                    true},
                                                   nullptr,
                                                   scatter(LOG_MUSHROOMS)}};
    // the bush's leaves are oak leaves; a single variation_chance stands for every layer
    static const Tree::SimpleCanopy BUSH_CANOPY = {-1, 1, 0, 1.0f, {{1, 2}, {1, 2}, {1, 2}}};
    static const TreeDefinition JUNGLE_BUSH = {{"minecraft:jungle_bush_feature",
                                                Tree::Trunk::Simple,
                                                &VanillaBlocks::JUNGLE_LOG,
                                                {1, 1},
                                                {0, 0},
                                                Tree::Canopy::Simple,
                                                &VanillaBlocks::OAK_LEAVES,
                                                {0, 0},
                                                {0, 0},
                                                {0, 0},
                                                0,
                                                0.0f,
                                                false,
                                                0,
                                                0,
                                                0,
                                                0,
                                                0,
                                                false,
                                                false,
                                                &BUSH_CANOPY},
                                               nullptr,
                                               {}};
    static const Tree::Mega MEGA_JUNGLE = {2,
                                           10,
                                           {3, 20},
                                           {0, 0},
                                           0,
                                           0.0f,
                                           0,
                                           &VanillaBlocks::DIRT,
                                           0,
                                           0,
                                           {{3, 3}, 2, 2, false},
                                           5,
                                           0.5f,
                                           {2, 6},
                                           0.5f,
                                           1.0f,
                                           {{2, 4}, 1, 0, true},
                                           1,
                                           3};
    static const TreeDefinition MEGA_JUNGLE_TREE = {{"minecraft:mega_jungle_tree_feature",
                                                     Tree::Trunk::Mega,
                                                     &VanillaBlocks::JUNGLE_LOG,
                                                     {0, 0},
                                                     {0, 0},
                                                     Tree::Canopy::Mega,
                                                     &VanillaBlocks::JUNGLE_LEAVES,
                                                     {0, 0},
                                                     {0, 0},
                                                     {0, 0},
                                                     0,
                                                     0.0f,
                                                     false},
                                                    nullptr,
                                                    {},
                                                    &MEGA_JUNGLE};
    static const SingleBlockDefinition COCOA[] = {{"minecraft:cocoa_age_0_feature", &VanillaBlocks::COCOA, false, false,
                                                   -1, false, false, &VanillaBlocks::JUNGLE_LOG, 0},
                                                  {"minecraft:cocoa_age_1_feature", &VanillaBlocks::COCOA, false, false,
                                                   -1, false, false, &VanillaBlocks::JUNGLE_LOG, 1},
                                                  {"minecraft:cocoa_age_2_feature", &VanillaBlocks::COCOA, false, false,
                                                   -1, false, false, &VanillaBlocks::JUNGLE_LOG, 2}};
    static const WeightedDefinition SELECT_COCOA = {"minecraft:select_cocoa_feature", [] {
                                                        std::vector<std::pair<Placed, int32_t>> features;
                                                        for (const SingleBlockDefinition &cocoa: COCOA) {
                                                            Placed placed;
                                                            placed.mSingleBlock = &cocoa;
                                                            features.emplace_back(placed, 1);
                                                        }
                                                        return features;
                                                    }()};
    static const Placed COCOA_PICK = [] {
        Placed placed;
        placed.mWeighted = &SELECT_COCOA;
        return placed;
    }();
    static const ScatterDefinition OPTIONAL_COCOA[] = {
            optional("minecraft:optional_lower_cocoa_feature", COCOA_PICK, 4),
            optional("minecraft:optional_upper_cocoa_feature", COCOA_PICK, 3)};
    // the grid walks x over 0, 1, 2, 0 and z over 1, 0, 0, -1 from the position the tree hands on
    static const ScatterDefinition::AxisText COCOA_X = {FIXED_GRID, "-1", "1", 2, 1};
    static const ScatterDefinition::AxisText COCOA_Z = {FIXED_GRID, "-1", "1", 1, 0};
    static const ScatterDefinition COCOA_SCATTERS[] = {{"minecraft:scatter_jungle_tree_lower_cocoa_feature",
                                                        "4",
                                                        {COCOA_X, {0, "-5", ""}, COCOA_Z},
                                                        0,
                                                        scatter(OPTIONAL_COCOA[0]),
                                                        0,
                                                        0,
                                                        "100.0"},
                                                       {"minecraft:scatter_jungle_tree_upper_cocoa_feature",
                                                        "4",
                                                        {COCOA_X, {0, "-4", ""}, COCOA_Z},
                                                        0,
                                                        scatter(OPTIONAL_COCOA[1]),
                                                        0,
                                                        0,
                                                        "100.0"}};
    static const AggregateDefinition SCATTER_COCOA = {
            "minecraft:scatter_jungle_tree_cocoa_feature", {scatter(COCOA_SCATTERS[0]), scatter(COCOA_SCATTERS[1])}, 0};
    static const ScatterDefinition OPTIONAL_JUNGLE[] = {
            optional("minecraft:optional_fallen_jungle_tree_feature", tree(JUNGLE_TREES[1]), 80),
            optional("minecraft:optional_jungle_tree_cocoa_feature", aggregate(SCATTER_COCOA), 5)};
    static const SequenceDefinition JUNGLE_WITH_COCOA = {"minecraft:jungle_tree_with_cocoa_feature",
                                                         {tree(JUNGLE_TREES[0]), scatter(OPTIONAL_JUNGLE[1])}};
    // its third feature, noop_undecorated_jungle_tree_feature, has 0 iterations and draws nothing
    static const AggregateDefinition SELECT_JUNGLE = {"minecraft:select_jungle_tree_feature",
                                                      {scatter(OPTIONAL_JUNGLE[0]),
                                                       [] {
                                                           Placed placed;
                                                           placed.mSequence = &JUNGLE_WITH_COCOA;
                                                           return placed;
                                                       }()},
                                                      2};
    // 0 iterations in the features the game loads
    static const ScatterDefinition TALL_GRASS_AROUND_TREE = {"minecraft:scatter_tall_grass_around_tree_feature",
                                                             "0",
                                                             {{6, "-3", "3"}, {6, "-4", "4"}, {6, "-3", "3"}},
                                                             5,
                                                             single(0)};

    // savanna_tree_feature: a diagonal acacia trunk of 5 plus up to 2 plus up to 2, its acacia_canopy of 3 and one
    // branch with a simplified canopy of 2
    static const Tree::Acacia SAVANNA = {1, 5, {3, 3}, 0, {1, 5}, {1, 4}, {0, 0}, true, {1, 4}, {1, 3}, 1,
                                         1, 2, true,   0, 0,      0,      0,      0,    0,      3};
    static const TreeDefinition SAVANNA_TREE = {{"minecraft:savanna_tree_feature",
                                                 Tree::Trunk::Acacia,
                                                 &VanillaBlocks::ACACIA_LOG,
                                                 {0, 0},
                                                 {0, 0},
                                                 Tree::Canopy::Acacia,
                                                 &VanillaBlocks::ACACIA_LEAVES,
                                                 {0, 0},
                                                 {0, 0},
                                                 {0, 0},
                                                 0,
                                                 0.0f,
                                                 false},
                                                nullptr,
                                                {},
                                                nullptr,
                                                &SAVANNA};

    // mangrove_tree_feature and tall_mangrove_tree_feature, their random pick and their beenest aggregates
    static const Tree::Mangrove MANGROVES[] = {
            {8, 15, {1, 4}, 1, 2, 2, 1, 4, {0, 1}, {1, 4}, {2, 2}, {3, 3}, 70, 1, 4, 4, 1, 4},
            {8, 15, {5, 8}, 1, 2, 4, 1, 9, {0, 1}, {1, 6}, {2, 2}, {3, 3}, 70, 1, 4, 4, 1, 7}};
    static const auto mangrove = [](const char *identifier, const Tree::Mangrove &shape) {
        TreeDefinition definition = {{identifier,
                                      Tree::Trunk::Mangrove,
                                      &VanillaBlocks::MANGROVE_LOG,
                                      {0, 0},
                                      {0, 0},
                                      Tree::Canopy::Mangrove,
                                      &VanillaBlocks::MANGROVE_LEAVES,
                                      {0, 0},
                                      {0, 0},
                                      {0, 0},
                                      0,
                                      0.0f,
                                      false},
                                     nullptr,
                                     {}};
        definition.mTree.mMangrove = &shape;
        return definition;
    };
    static const TreeDefinition MANGROVE_TREES[] = {mangrove("minecraft:mangrove_tree_feature", MANGROVES[0]),
                                                    mangrove("minecraft:tall_mangrove_tree_feature", MANGROVES[1])};
    static const WeightedDefinition RANDOM_MANGROVE = {"minecraft:random_mangrove_tree_feature",
                                                       {{tree(MANGROVE_TREES[1]), 85}, {tree(MANGROVE_TREES[0]), 15}}};
    static const AggregateDefinition MANGROVES_WITH_BEENEST[] = {
            {"minecraft:mangrove_tree_with_beenest_feature", {tree(MANGROVE_TREES[0]), BEEHIVE_SEARCH}, 1},
            {"minecraft:tall_mangrove_tree_with_beenest_feature", {tree(MANGROVE_TREES[1]), BEEHIVE_SEARCH}, 1}};

    // cherry_tree_feature, alone 19 times in 20 and otherwise with its beehive search
    static const Tree::Cherry CHERRY = {7,      {1},    {10, 10, 10}, {-4, -3}, {-1, 0},  {2, 4},
                                        {5, 5}, {4, 4}, 25.0f,        50.0f,    16.6666f, 33.3333f};
    static const TreeDefinition CHERRY_TREE = [] {
        TreeDefinition definition = {{"minecraft:cherry_tree_feature",
                                      Tree::Trunk::Cherry,
                                      &VanillaBlocks::CHERRY_LOG,
                                      {0, 0},
                                      {0, 0},
                                      Tree::Canopy::Cherry,
                                      &VanillaBlocks::CHERRY_LEAVES,
                                      {0, 0},
                                      {0, 0},
                                      {0, 0},
                                      0,
                                      0.0f,
                                      false},
                                     nullptr,
                                     {}};
        definition.mTree.mCherry = &CHERRY;
        return definition;
    }();
    static const AggregateDefinition CHERRY_WITH_BEEHIVE = {
            "minecraft:cherry_tree_with_beehive_feature", {tree(CHERRY_TREE), BEEHIVE_SEARCH}, 1};
    static const WeightedDefinition RANDOM_CHERRY = {"minecraft:random_cherry_tree_feature",
                                                     {{tree(CHERRY_TREE), 19}, {aggregate(CHERRY_WITH_BEEHIVE), 1}}};

    static const ScatterDefinition FIXUP_VINES = {
            "minecraft:fixup_vines_position_feature", "1", {{0, "8", ""}, ZERO, {0, "8", ""}}, 0, [] {
                Placed placed;
                placed.mFunction = &placeVines;
                return placed;
            }()};

    using Pick = LegacyTreeDefinition::Pick;
    static const LegacyTreeDefinition LEGACY_TREES[] = {
            {"minecraft:legacy:forest_tree_feature",
             10.0f,
             Pick::Forest,
             {aggregate(WITH_LEAF_LITTER[0]), aggregate(WITH_LEAF_LITTER[1]), aggregate(WITH_LEAF_LITTER[2])}},
            {"minecraft:legacy:ice_tree_feature", 0.1f, Pick::First, {aggregate(SELECT_SPRUCE)}},
            // types 1 and 2 of the constructor; the mutated forest's second feature, select_super_birch_tree_feature,
            // is in none of the game's packs and places nothing
            {"minecraft:legacy:birch_forest_tree_feature", 10.0f, Pick::First, {aggregate(SELECT_BIRCH)}},
            {"minecraft:legacy:birch_forest_mutated_tree_feature",
             10.0f,
             Pick::BirchMutated,
             {aggregate(SELECT_BIRCH)}},
            {"minecraft:legacy:plains_tree_feature",
             0.05f,
             Pick::OneInThree,
             {aggregate(FANCY_WITH_BEEHIVE), aggregate(SELECT_OAK)}},
            {"minecraft:legacy:taiga_tree_feature",
             10.0f,
             Pick::OneInThree,
             {tree(TREES[9]), aggregate(SELECT_SPRUCE)}},
            {"minecraft:legacy:mega_taiga_tree_feature",
             10.0f,
             Pick::MegaTaiga,
             {tree(MEGA_TREES[0]), tree(MEGA_TREES[1]), tree(TREES[9]), aggregate(SELECT_SPRUCE)}},
            // type 16 of the constructor
            {"minecraft:legacy:redwood_taiga_mutated_tree_feature",
             10.0f,
             Pick::RedwoodMutated,
             {tree(MEGA_TREES[0]), tree(TREES[9]), aggregate(SELECT_SPRUCE)}},
            // the swamp foliage feature (constructor sub_14D21E9F0, place sub_14D21F070) counts its trees like a
            // legacy tree feature of density 2
            {"minecraft:legacy:swamp_foliage_feature",
             2.0f,
             Pick::Swamp,
             {HUGE_MUSHROOM, tree(SWAMP_TREE)},
             &SEAGRASS[3]},
            // types 8, 9 and 0 of the constructor
            {"minecraft:legacy:jungle_tree_feature",
             50.0f,
             Pick::Jungle,
             {tree(TREES[2]), tree(JUNGLE_BUSH), tree(MEGA_JUNGLE_TREE), aggregate(SELECT_JUNGLE)}},
            {"minecraft:legacy:jungle_edge_tree_feature",
             2.0f,
             Pick::JungleEdge,
             {aggregate(FANCY_WITH_BEEHIVE), tree(JUNGLE_BUSH), aggregate(SELECT_JUNGLE)}},
            // types 12 and 13
            {"minecraft:legacy:savanna_tree_feature", 1.0f, Pick::Savanna, {tree(SAVANNA_TREE), aggregate(SELECT_OAK)}},
            {"minecraft:legacy:savanna_mutated_tree_feature",
             2.0f,
             Pick::Savanna,
             {tree(SAVANNA_TREE), aggregate(SELECT_OAK)}},
            {"minecraft:legacy:bamboo_jungle_tree_feature",
             30.0f,
             Pick::BambooJungle,
             {tree(TREES[2]), tree(JUNGLE_BUSH), tree(MEGA_JUNGLE_TREE), scatter(TALL_GRASS_AROUND_TREE)}}};

    // the double plant patches: a lower half, then the upper one placed one block above it
    static const AxisText UP = {0, "1", ""};
    static const ScatterDefinition UPPER_HALVES[] = {
            {"minecraft:fixup_fern_double_plant_upper_position_feature", "1", {ZERO, UP, ZERO}, 0, single(3)},
            {"minecraft:fixup_grass_double_plant_upper_position_feature", "1", {ZERO, UP, ZERO}, 0, single(5)}};
    static const AggregateDefinition DOUBLE_PLANTS[] = {
            {"minecraft:fern_double_plant_feature", {single(2), scatter(UPPER_HALVES[0])}, 1},
            {"minecraft:grass_double_plant_feature", {single(4), scatter(UPPER_HALVES[1])}, 1}};
    static const ScatterDefinition DOUBLE_PLANT_PATCHES[] = {{"minecraft:fern_double_plant_patch_feature",
                                                              "64",
                                                              {{6, "-8", "8"}, {6, "-4", "4"}, {6, "-8", "8"}},
                                                              5,
                                                              aggregate(DOUBLE_PLANTS[0])},
                                                             {"minecraft:grass_double_plant_patch_feature",
                                                              "32",
                                                              {{6, "-7", "7"}, {6, "-3", "3"}, {6, "-7", "7"}},
                                                              5,
                                                              aggregate(DOUBLE_PLANTS[1])}};

    // BDS 1.26.61 behavior packs, features/underwater_magma_*.json
    static const BlobDefinition UNDERWATER_MAGMA = {"minecraft:underwater_magma_feature", &VanillaBlocks::MAGMA, 1,
                                                    0.5f, 1};
    static const SnapDefinition UNDERWATER_MAGMA_SNAP = {"minecraft:underwater_magma_snap_to_surface_feature",
                                                         blob(UNDERWATER_MAGMA),
                                                         5,
                                                         1,
                                                         false,
                                                         false,
                                                         true,
                                                         false,
                                                         {}};
    static const ThresholdDefinition UNDERWATER_MAGMA_UNDERGROUND = {"minecraft:underwater_magma_underground_feature",
                                                                     snap(UNDERWATER_MAGMA_SNAP), 2};

    // BDS 1.26.61 behavior packs, features/*glow_lichen_feature.json: the ceiling and the four walls
    static const MultifaceDefinition GLOW_LICHEN = {"minecraft:glow_lichen_feature",
                                                    &VanillaBlocks::GLOW_LICHEN,
                                                    20,
                                                    {1, 2, 3, 4, 5},
                                                    0.5f,
                                                    {"minecraft:stone", "minecraft:andesite", "minecraft:diorite",
                                                     "minecraft:granite", "minecraft:dripstone_block",
                                                     "minecraft:calcite", "minecraft:tuff", "minecraft:deepslate",
                                                     "minecraft:sulfur", "minecraft:cinnabar"}};
    static const ThresholdDefinition UNDERGROUND_GLOW_LICHEN = {"minecraft:underground_glow_lichen_feature",
                                                                multiface(GLOW_LICHEN), 2};

    // BDS 1.26.61 behavior packs, features/small_dripstone*.json; the scatter keeps the default xyz order
    // sub_14728CE10
    static const DripstoneDefinition DRIPSTONE = {&dripstoneBlocks, 3, 5, {0.1f, 0.3f, 0.1f, 1.0f}};
    static const DripstoneDefinition SULFUR_SPIKES = {&sulfurSpikeBlocks, 1, 4, {0.0f, 0.0f, 0.0f, 0.0f}};
    static const Placed POINTED_DRIPSTONE = [] {
        Placed placed;
        placed.mPointedDripstone = &DRIPSTONE;
        return placed;
    }();
    static const SnapDefinition SMALL_DRIPSTONE_SNAP = {"minecraft:small_dripstone_snap_to_surface_feature",
                                                        POINTED_DRIPSTONE,
                                                        12,
                                                        3,
                                                        true,
                                                        false,
                                                        true,
                                                        false,
                                                        {}};
    static const ScatterDefinition SMALL_DRIPSTONE = {"minecraft:small_dripstone_feature",
                                                      "Math.Random(1, 5)",
                                                      {{2, "-10", "10"}, {2, "-2", "2"}, {2, "-10", "10"}},
                                                      0,
                                                      snap(SMALL_DRIPSTONE_SNAP)};

    // BDS 1.26.61 behavior packs, features/amethyst_geode_feature.json
    static const GeodeDefinition GEODES[] = {{"minecraft:amethyst_geode_feature",
                                              &VanillaBlocks::AIR,
                                              &VanillaBlocks::AMETHYST_BLOCK,
                                              &VanillaBlocks::BUDDING_AMETHYST,
                                              &VanillaBlocks::CALCITE,
                                              &VanillaBlocks::SMOOTH_BASALT,
                                              {&VanillaBlocks::AMETHYST_CLUSTER, &VanillaBlocks::LARGE_AMETHYST_BUD,
                                               &VanillaBlocks::MEDIUM_AMETHYST_BUD, &VanillaBlocks::SMALL_AMETHYST_BUD},
                                              4,
                                              7,
                                              3,
                                              5,
                                              1,
                                              3,
                                              16,
                                              2,
                                              0.025f,
                                              0.35f,
                                              0.083f,
                                              true,
                                              1}};

    // BDS 1.26.61 behavior packs, features/moss_patch_feature.json, clay_*_with_dripleaves_feature.json and
    // cave_vine_feature.json; the floor rules run in after_surface_pass
    static const auto vegetationPatch = [](const VegetationPatchDefinition &definition) {
        Placed placed;
        placed.mVegetationPatch = &definition;
        return placed;
    };
    static const auto growingPlant = [](const GrowingPlantDefinition &definition) {
        Placed placed;
        placed.mGrowingPlant = &definition;
        return placed;
    };
    static const std::vector<std::string> MOSS_REPLACEABLE = {"minecraft:dirt",
                                                              "minecraft:coarse_dirt",
                                                              "minecraft:podzol",
                                                              "minecraft:dirt_with_roots",
                                                              "minecraft:grass_block",
                                                              "minecraft:mycelium",
                                                              "minecraft:stone",
                                                              "minecraft:granite",
                                                              "minecraft:diorite",
                                                              "minecraft:andesite",
                                                              "minecraft:deepslate",
                                                              "minecraft:tuff",
                                                              "minecraft:moss_block",
                                                              "minecraft:cave_vines",
                                                              "minecraft:cave_vines_body_with_berries",
                                                              "minecraft:cave_vines_head_with_berries"};
    // moss_vegetation_feature, the weighted blocks built in the registrar sub_14738C180 (total 96)
    static const std::vector<std::pair<Block (*)(), int32_t>> MOSS_VEGETATION = {{&VanillaBlocks::FLOWERING_AZALEA, 4},
                                                                                 {&VanillaBlocks::AZALEA, 7},
                                                                                 {&VanillaBlocks::MOSS_CARPET, 25},
                                                                                 {&VanillaBlocks::SHORT_GRASS, 50},
                                                                                 {&VanillaBlocks::TALL_GRASS, 10}};
    static const VegetationPatchDefinition MOSS_PATCH = {"minecraft:moss_patch_feature",
                                                         &VanillaBlocks::MOSS_BLOCK,
                                                         MOSS_REPLACEABLE,
                                                         1,
                                                         2,
                                                         5,
                                                         0.0f,
                                                         0.3f,
                                                         4,
                                                         8,
                                                         true,
                                                         MOSS_VEGETATION,
                                                         {},
                                                         0.8f,
                                                         false};
    static const SnapDefinition MOSS_PATCH_SNAP = {"minecraft:moss_patch_snap_to_floor_feature",
                                                   vegetationPatch(MOSS_PATCH),
                                                   12,
                                                   1,
                                                   true,
                                                   false,
                                                   false,
                                                   false,
                                                   {}};
    static const std::vector<std::string> CLAY_REPLACEABLE = {"minecraft:clay",
                                                              "minecraft:sand",
                                                              "minecraft:gravel",
                                                              "minecraft:dirt",
                                                              "minecraft:coarse_dirt",
                                                              "minecraft:podzol",
                                                              "minecraft:dirt_with_roots",
                                                              "minecraft:grass_block",
                                                              "minecraft:mycelium",
                                                              "minecraft:stone",
                                                              "minecraft:deepslate",
                                                              "minecraft:tuff",
                                                              "minecraft:moss_block",
                                                              "minecraft:cave_vines",
                                                              "minecraft:cave_vines_body_with_berries",
                                                              "minecraft:cave_vines_head_with_berries"};
    // dripleaf_feature is the game's own DripleafFeature (sub_14D208A20), not a data feature
    static const auto dripleaf = [] {
        Placed placed;
        placed.mFunction = &OverworldRulePlacer::_placeDripleaf;
        return placed;
    };
    static const VegetationPatchDefinition CLAY_PATCH = {"minecraft:clay_with_dripleaves_feature",
                                                         &VanillaBlocks::CLAY,
                                                         CLAY_REPLACEABLE,
                                                         3,
                                                         3,
                                                         2,
                                                         0.8f,
                                                         0.7f,
                                                         4,
                                                         8,
                                                         true,
                                                         {},
                                                         dripleaf(),
                                                         0.05f,
                                                         false};
    static const VegetationPatchDefinition CLAY_POOL = {"minecraft:clay_pool_with_dripleaves_feature",
                                                        &VanillaBlocks::CLAY,
                                                        CLAY_REPLACEABLE,
                                                        3,
                                                        3,
                                                        5,
                                                        0.8f,
                                                        0.7f,
                                                        4,
                                                        8,
                                                        true,
                                                        {},
                                                        dripleaf(),
                                                        0.1f,
                                                        true};
    static const WeightedDefinition RANDOM_CLAY = {"minecraft:random_clay_with_dripleaves_feature",
                                                   {{vegetationPatch(CLAY_PATCH), 1}, {vegetationPatch(CLAY_POOL), 1}}};
    static const auto weightedOf = [](const WeightedDefinition &definition) {
        Placed placed;
        placed.mWeighted = &definition;
        return placed;
    };
    static const SnapDefinition CLAY_SNAP = {"minecraft:random_clay_with_dripleaves_snap_to_floor_feature",
                                             weightedOf(RANDOM_CLAY),
                                             12,
                                             1,
                                             true,
                                             false,
                                             false,
                                             false,
                                             {}};

    // cave_vine_feature and cave_vine_in_moss_feature (growing_plant), the weighted body and head built from
    // cave_vine_feature.json; both grow down and age 17..25
    static const std::vector<GrowingPlantDefinition::BlockWeight> CAVE_VINE_BODY = {
            {VanillaBlocks::CAVE_VINES().toBlockState(), 4},
            {VanillaBlocks::CAVE_VINES_BODY_WITH_BERRIES().toBlockState(), 1}};
    static const std::vector<GrowingPlantDefinition::BlockWeight> CAVE_VINE_HEAD = {
            {VanillaBlocks::CAVE_VINES().toBlockState(), 4},
            {VanillaBlocks::CAVE_VINES_HEAD_WITH_BERRIES().toBlockState(), 1}};
    static const GrowingPlantDefinition CAVE_VINE = {"minecraft:cave_vine_feature",
                                                     {{1, 13, 2}, {1, 2, 3}, {1, 7, 10}},
                                                     true,
                                                     17,
                                                     26,
                                                     CAVE_VINE_BODY,
                                                     CAVE_VINE_HEAD,
                                                     false};
    static const GrowingPlantDefinition CAVE_VINE_IN_MOSS = {"minecraft:cave_vine_in_moss_feature",
                                                             {{1, 4, 5}, {2, 7, 1}},
                                                             true,
                                                             17,
                                                             26,
                                                             CAVE_VINE_BODY,
                                                             CAVE_VINE_HEAD,
                                                             false};
    static const SnapDefinition CAVE_VINE_SNAP = {"minecraft:cave_vine_snap_to_ceiling_feature",
                                                  growingPlant(CAVE_VINE),
                                                  12,
                                                  0,
                                                  true,
                                                  false,
                                                  false,
                                                  false,
                                                  {}};
    static const std::vector<std::string> MOSS_CEILING_REPLACEABLE = {"minecraft:dirt",
                                                                      "minecraft:coarse_dirt",
                                                                      "minecraft:podzol",
                                                                      "minecraft:dirt_with_roots",
                                                                      "minecraft:grass_block",
                                                                      "minecraft:mycelium",
                                                                      "minecraft:stone",
                                                                      "minecraft:granite",
                                                                      "minecraft:polished_granite",
                                                                      "minecraft:diorite",
                                                                      "minecraft:polished_diorite",
                                                                      "minecraft:andesite",
                                                                      "minecraft:polished_andesite",
                                                                      "minecraft:deepslate",
                                                                      "minecraft:tuff",
                                                                      "minecraft:moss_block",
                                                                      "minecraft:cave_vines",
                                                                      "minecraft:cave_vines_body_with_berries",
                                                                      "minecraft:cave_vines_head_with_berries"};
    static const VegetationPatchDefinition MOSS_CEILING = {"minecraft:moss_ceiling_feature",
                                                           &VanillaBlocks::MOSS_BLOCK,
                                                           MOSS_CEILING_REPLACEABLE,
                                                           1,
                                                           3,
                                                           5,
                                                           0.0f,
                                                           0.3f,
                                                           4,
                                                           8,
                                                           false,
                                                           {},
                                                           growingPlant(CAVE_VINE_IN_MOSS),
                                                           0.08f,
                                                           false};
    static const SnapDefinition MOSS_CEILING_SNAP = {"minecraft:moss_ceiling_snap_to_ceiling_feature",
                                                     vegetationPatch(MOSS_CEILING),
                                                     12,
                                                     0,
                                                     true,
                                                     false,
                                                     false,
                                                     false,
                                                     {}};

    // features/sulfur_pool_with_potent_sulfur*.json; sulfur_pool is internal
    static const auto function = [](bool (*place)(BlockManager &, int32_t, int32_t, int32_t, std::mt19937 &)) {
        Placed placed;
        placed.mFunction = place;
        return placed;
    };
    static const AggregateDefinition SULFUR_POOL_WITH_POTENT_SULFUR = {
            "minecraft:sulfur_pool_with_potent_sulfur",
            {function(&OverworldRulePlacer::_placeSulfurPool),
             function(&OverworldRulePlacer::_placePotentSulfurSearch)},
            1};
    static const SnapDefinition SULFUR_POOL_SNAP = {"minecraft:sulfur_pool_with_potent_sulfur_snap_to_surface_feature",
                                                    aggregate(SULFUR_POOL_WITH_POTENT_SULFUR),
                                                    32,
                                                    1,
                                                    true,
                                                    false,
                                                    true,
                                                    true,
                                                    {"minecraft:sulfur"}};
    // BDS 1.26.61 behavior packs and definitions/features: roofed_tree_feature, roofed_tree_with_vines_feature and
    // the dark forest's random_roofed_forest_feature
    static const Tree::Acacia ROOFED = {2, 6, {3, 2}, 3,    {0, 4}, {0, 3}, {0, 0}, false, {2, 5}, {1, 1},
                                        1, 3, 2,      true, 0,      0,      4,      2,     2,      3};
    static const Tree::Acacia ROOFED_WITH_VINES = {2, 6, {3, 2}, 3,    {0, 4}, {0, 3}, {0, 0}, false, {2, 5}, {1, 1},
                                                   1, 3, 2,      true, 6,      7,      4,      2,     2,      3};
    static const TreeDefinition ROOFED_TREES[] = {{{"minecraft:roofed_tree_feature",
                                                    Tree::Trunk::Acacia,
                                                    &VanillaBlocks::DARK_OAK_LOG,
                                                    {0, 0},
                                                    {0, 0},
                                                    Tree::Canopy::Roofed,
                                                    &VanillaBlocks::DARK_OAK_LEAVES,
                                                    {0, 0},
                                                    {0, 0},
                                                    {0, 0},
                                                    0,
                                                    0.0f,
                                                    false},
                                                   nullptr,
                                                   {},
                                                   nullptr,
                                                   &ROOFED},
                                                  {{"minecraft:roofed_tree_with_vines_feature",
                                                    Tree::Trunk::Acacia,
                                                    &VanillaBlocks::DARK_OAK_LOG,
                                                    {0, 0},
                                                    {0, 0},
                                                    Tree::Canopy::Roofed,
                                                    &VanillaBlocks::DARK_OAK_LEAVES,
                                                    {0, 0},
                                                    {0, 0},
                                                    {0, 0},
                                                    0,
                                                    0.0f,
                                                    false,
                                                    0,
                                                    1,
                                                    0,
                                                    0,
                                                    0,
                                                    false,
                                                    true},
                                                   nullptr,
                                                   {},
                                                   nullptr,
                                                   &ROOFED_WITH_VINES}};
    static const ScatterDefinition OPTIONAL_ROOFED_WITH_VINES = optional(
            "minecraft:optional_roofed_tree_with_vines_feature", tree(ROOFED_TREES[1]), 6);
    static const AggregateDefinition SELECT_ROOFED = {
            "minecraft:select_roofed_tree_feature", {scatter(OPTIONAL_ROOFED_WITH_VINES), tree(ROOFED_TREES[0])}, 2};
    static const WeightedDefinition RANDOM_ROOFED = {"minecraft:random_roofed_forest_feature",
                                                     {{HUGE_MUSHROOM, 75},
                                                      {aggregate(SELECT_ROOFED), 630},
                                                      {aggregate(SELECT_BIRCH), 70},
                                                      {tree(TREES[2]), 30},
                                                      {aggregate(SELECT_OAK), 195}}};
    static const AggregateDefinition ROOFED_WITH_LEAF_LITTER = {
            "minecraft:random_roofed_forest_feature_with_leaf_litter_feature",
            {weightedOf(RANDOM_ROOFED), scatter(LEAF_LITTER_SCATTER)},
            1};
    static const ScatterDefinition TALL_GRASS_AROUND_FOLIAGE = {
            "minecraft:scatter_tall_grass_around_forest_foliage_feature",
            "20",
            {{6, "-3", "3"}, {6, "-4", "4"}, {6, "-3", "3"}},
            5,
            single(0)};
    static const AggregateDefinition ROOFED_FOREST = {
            "minecraft:random_roofed_forest_feature_with_decoration_feature",
            {aggregate(ROOFED_WITH_LEAF_LITTER), scatter(TALL_GRASS_AROUND_FOLIAGE)},
            0};

    // vanilla_1.26.50 features: the dappled forest's poplars, their shelf mushrooms and the forest's picks
    static const ScatterDefinition OPTIONAL_SHELF_MUSHROOM = {"minecraft:optional_shelf_mushroom_feature",
                                                              "1",
                                                              {ZERO, ZERO, ZERO},
                                                              0,
                                                              function(&placeShelfMushroom),
                                                              1,
                                                              4};
    static const WeightedDefinition DAPPLED_FALLEN_DECORATION = {
            "minecraft:select_dappled_forest_fallen_tree_decoration",
            {{scatter(OPTIONAL_SHELF_MUSHROOM), 1}, {scatter(LOG_BROWN_MUSHROOM), 1}}};
    static const Tree::Poplar POPLAR = {{4, 4}, {1, 4}, {{5, 5}, {6, 5}, {7, 1}, {8, 1}}, {5, 6}, 0.15f};
    static const auto poplar = [](const char *identifier, Block (*leaves)()) {
        Tree::Definition definition = {identifier,
                                       Tree::Trunk::Poplar,
                                       &VanillaBlocks::POPLAR_LOG,
                                       {7, 11},
                                       {0, 0},
                                       Tree::Canopy::None,
                                       leaves,
                                       {0, 0},
                                       {0, 0},
                                       {0, 0},
                                       0,
                                       0.0f,
                                       true};
        definition.mPoplar = &POPLAR;
        return TreeDefinition{definition, nullptr, scatter(OPTIONAL_SHELF_MUSHROOM)};
    };
    static const TreeDefinition POPLAR_TREES[] = {
            poplar("minecraft:red_poplar_tree_feature", &VanillaBlocks::RED_POPLAR_LEAVES),
            poplar("minecraft:orange_poplar_tree_feature", &VanillaBlocks::ORANGE_POPLAR_LEAVES),
            poplar("minecraft:yellow_poplar_tree_feature", &VanillaBlocks::YELLOW_POPLAR_LEAVES),
            {{"minecraft:fallen_poplar_tree_feature",
              Tree::Trunk::Fallen,
              &VanillaBlocks::POPLAR_LOG,
              {4, 7},
              {0, 0},
              Tree::Canopy::None,
              nullptr,
              {0, 0},
              {0, 0},
              {0, 0},
              0,
              0.0f,
              true},
             nullptr,
             weightedOf(DAPPLED_FALLEN_DECORATION)}};
    static const AggregateDefinition POPLARS_WITH_LEAF_LITTER[] = {
            {"minecraft:red_poplar_tree_with_leaf_litter_feature",
             {tree(POPLAR_TREES[0]), scatter(LEAF_LITTER_SCATTER)},
             1},
            {"minecraft:orange_poplar_tree_with_leaf_litter_feature",
             {tree(POPLAR_TREES[1]), scatter(LEAF_LITTER_SCATTER)},
             1},
            {"minecraft:yellow_poplar_tree_with_leaf_litter_feature",
             {tree(POPLAR_TREES[2]), scatter(LEAF_LITTER_SCATTER)},
             1}};
    static const WeightedDefinition DAPPLED_FOREST_TREES = {"minecraft:select_tree_dappled_forest_feature",
                                                            {{aggregate(POPLARS_WITH_LEAF_LITTER[0]), 200},
                                                             {aggregate(POPLARS_WITH_LEAF_LITTER[1]), 240},
                                                             {aggregate(POPLARS_WITH_LEAF_LITTER[2]), 90},
                                                             {aggregate(STANDING_SPRUCE), 27},
                                                             {tree(POPLAR_TREES[3]), 120}}};
    // no coordinate_eval_order: xzy
    static const ScatterDefinition RED_SHRUBS = {"minecraft:scatter_red_shrub_bush_feature",
                                                 "8",
                                                 {{6, "-7", "7"}, {6, "-3", "3"}, {6, "-7", "7"}},
                                                 1,
                                                 single(14)};
    static const ScatterDefinition BROWN_MUSHROOMS = {"minecraft:scatter_brown_mushroom_feature",
                                                      "96",
                                                      {{6, "-7", "7"}, {6, "-3", "3"}, {6, "-7", "7"}},
                                                      5,
                                                      mushroom(&VanillaBlocks::BROWN_MUSHROOM)};

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
            {"minecraft:basalt_columns_feature", &NetherFeatures::placeBasaltColumns}};

    // BDS 1.26.61 behavior packs, features/*fossil_feature.json
    static const std::vector<FossilDefinition> FOSSILS = {
            {"minecraft:fossil_feature", "minecraft:coal_ore", 4},
            {"minecraft:deepslate_diamond_fossil_feature", "minecraft:deepslate_diamond_ore", 4}};

    Feature feature;
    if (entry.mPass == AFTER_SURFACE_PASS) {
        // vtable 0x14F4A0F60, place sub_14D217680
        feature.mKelp = entry.mFeature == "minecraft:kelp_feature";
        if (entry.mFeature == UNDERWATER_MAGMA_UNDERGROUND.mIdentifier)
            feature.mPlaced.mThreshold = &UNDERWATER_MAGMA_UNDERGROUND;
        if (entry.mFeature == SMALL_DRIPSTONE.mIdentifier)
            feature.mPlaced.mScatter = &SMALL_DRIPSTONE;
        if (entry.mFeature == MOSS_PATCH_SNAP.mIdentifier)
            feature.mPlaced.mSnap = &MOSS_PATCH_SNAP;
        if (entry.mFeature == CLAY_SNAP.mIdentifier)
            feature.mPlaced.mSnap = &CLAY_SNAP;
        if (entry.mFeature == MOSS_CEILING_SNAP.mIdentifier)
            feature.mPlaced.mSnap = &MOSS_CEILING_SNAP;
        if (entry.mFeature == CAVE_VINE_SNAP.mIdentifier)
            feature.mPlaced.mSnap = &CAVE_VINE_SNAP;
        if (entry.mFeature == "minecraft:dripstone_cluster_feature")
            feature.mPlaced.mDripstoneCluster = &DRIPSTONE;
        if (entry.mFeature == "minecraft:sulfur_spike_cluster_feature")
            feature.mPlaced.mDripstoneCluster = &SULFUR_SPIKES;
        if (entry.mFeature == "minecraft:sulfur_spike_feature")
            feature.mPlaced.mPointedDripstone = &SULFUR_SPIKES;
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
        if (entry.mFeature == SULFUR_POOL_SNAP.mIdentifier)
            feature.mPlaced.mSnap = &SULFUR_POOL_SNAP;
        for (const NetherPlaced &nether: NETHER_FEATURES) {
            if (entry.mFeature == nether.mIdentifier)
                feature.mPlaced.mFunction = nether.mFunction;
        }
        feature.mSprings = entry.mFeature == "minecraft:legacy:springs_feature";
        if (entry.mFeature == ROOFED_FOREST.mIdentifier)
            feature.mPlaced.mAggregate = &ROOFED_FOREST;
        if (entry.mFeature == DAPPLED_FOREST_TREES.mIdentifier)
            feature.mPlaced.mWeighted = &DAPPLED_FOREST_TREES;
        if (entry.mFeature == RED_SHRUBS.mIdentifier)
            feature.mPlaced.mScatter = &RED_SHRUBS;
        if (entry.mFeature == RANDOM_MANGROVE.mIdentifier)
            feature.mPlaced.mWeighted = &RANDOM_MANGROVE;
        for (const AggregateDefinition &withBeenest: MANGROVES_WITH_BEENEST) {
            if (entry.mFeature == withBeenest.mIdentifier)
                feature.mPlaced.mAggregate = &withBeenest;
        }
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
    if (entry.mPass == AFTER_SURFACE_PASS && entry.mFeature == RANDOM_CHERRY.mIdentifier)
        feature.mPlaced.mWeighted = &RANDOM_CHERRY;
    if (entry.mPass == AFTER_SURFACE_PASS && entry.mFeature == FIXUP_VINES.mIdentifier)
        feature.mPlaced.mScatter = &FIXUP_VINES;
    if (entry.mPass == AFTER_SURFACE_PASS && entry.mFeature == BROWN_MUSHROOMS.mIdentifier)
        feature.mPlaced.mScatter = &BROWN_MUSHROOMS;
    if (entry.mPass == AFTER_SURFACE_PASS && entry.mFeature == "minecraft:bamboo_feature")
        feature.mPlaced.mFunction = &placeBamboo;
    if (entry.mPass == AFTER_SURFACE_PASS && entry.mFeature == "minecraft:melon_feature")
        feature.mPlaced.mFunction = &placeMelon;
    if (entry.mPass == AFTER_SURFACE_PASS && entry.mFeature == SINGLE_BLOCKS[12].mIdentifier)
        feature.mPlaced.mSingleBlock = &SINGLE_BLOCKS[12];
    // jigsaw feature pieces (sub_1469B1230), drawn from the post process random of their chunk
    if (entry.mPass.empty()) {
        static const auto function = [](bool (*place)(BlockManager &, int32_t, int32_t, int32_t, std::mt19937 &)) {
            Placed placed;
            placed.mFunction = place;
            return placed;
        };
        static const std::pair<const char *, Placed> JIGSAW_FEATURES[] = {
                {"minecraft:select_oak_tree_feature", aggregate(SELECT_OAK)},
                {"minecraft:select_spruce_tree_feature", aggregate(SELECT_SPRUCE)},
                {"minecraft:pine_tree_feature", tree(TREES[9])},
                {"minecraft:fern_feature", single(1)},
                {"minecraft:savanna_tree_feature", tree(SAVANNA_TREE)},
                {"minecraft:cactus_feature", function(&CactusFeature::placeCluster)},
                {"minecraft:hay_pile_feature", function(&BlockPileFeature::placeHay)},
                {"minecraft:melon_pile_feature", function(&BlockPileFeature::placeMelon)},
                {"minecraft:pumpkin_pile_feature", function(&BlockPileFeature::placePumpkin)},
                {"minecraft:snow_pile_feature", function(&BlockPileFeature::placeSnow)},
                {"minecraft:ice_pile_feature", function(&BlockPileFeature::placeIce)}};
        for (const auto &jigsaw: JIGSAW_FEATURES) {
            if (entry.mFeature == jigsaw.first)
                feature.mPlaced = jigsaw.second;
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

bool OverworldRulePlacer::placeFeature(BlockManager &blocks, const std::string &feature, int32_t x, int32_t y,
                                       int32_t z, std::mt19937 &random) {
    BiomeFeatureEntry entry;
    entry.mFeature = feature;
    const Feature found = _find(entry);
    if (found.mLegacyTree != nullptr) {
        _placeLegacyTrees(blocks, *found.mLegacyTree, x, y, z, random);
        return true;
    }
    if (!found.mPlaced.isSet())
        return false;

    _place(blocks, found.mPlaced, x, y, z, random);
    return true;
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
    _apply(blocks, chunk, &climate, levelSeed, biomes, nullptr);
}

void OverworldRulePlacer::applyBiome(BlockManager &blocks, const LevelChunk &chunk, uint32_t levelSeed, int32_t biome,
                                     std::mt19937 &decoration) {
    _apply(blocks, chunk, nullptr, levelSeed, {biome}, &decoration);
}

void OverworldRulePlacer::_apply(BlockManager &blocks, const LevelChunk &chunk, const OverworldClimate *climate,
                                 uint32_t levelSeed, const std::vector<int32_t> &biomes, std::mt19937 *decoration) {
    // the decoration seed of sub_14CDD4880, from the level seed and the chunk position
    std::mt19937 random(levelSeed);
    const uint32_t multiplierX = (random() >> 1) | 1u;
    const uint32_t multiplierZ = (random() >> 1) | 1u;
    const uint32_t decorationSeed = (multiplierZ * static_cast<uint32_t>(chunk.getZ())
                                     + multiplierX * static_cast<uint32_t>(chunk.getX()))
                                    ^ levelSeed;

    std::mt19937 ownDecoration(decorationSeed);
    placementContext.mDecoration = decoration != nullptr ? decoration : &ownDecoration;
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
    // an unported rule still draws math.random from the decoration random shared with the ported rules after it:
    // its chance, its iterations and, when they draw, its coordinates
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
    int32_t iterations = iterationsOf(entry.mChanceNumerator, entry.mChanceDenominator, percent, iterationsExpression,
                                      variables, scatter);
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
    // FossilFeature::place (sub_14D209B60); the game first gives up inside a structure's bounds, not tracked yet
    static const char *const TEMPLATES[] = {"fossils/fossil_spine_01.nbt", "fossils/fossil_spine_02.nbt",
                                            "fossils/fossil_spine_03.nbt", "fossils/fossil_spine_04.nbt",
                                            "fossils/fossil_skull_01.nbt", "fossils/fossil_skull_02.nbt",
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

    // the pieces turn around their first block: the far corner depends on the rotation
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

    // the game places the piece twice with one integrity seed: the second pass draws the same numbers, only
    // turns a tenth of the first's blocks into ore, and its match at chance 1 draws nothing
    std::mt19937 integrity(integritySeed);
    const BlockState ore(fossil.mOre);
    for (const StructureTemplate::BlockEntry &entry: piece->getBlocks()) {
        BlockState bone;
        if (!StructureBlockTable::translate(piece->getPalette()[static_cast<size_t>(entry.mPaletteIndex)], rotation,
                                            bone)
            || DecorationSupport::isAir(bone))
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
    if (placed.mVegetationPatch != nullptr)
        return _placeVegetationPatch(blocks, *placed.mVegetationPatch, x, y, z, random);
    if (placed.mGrowingPlant != nullptr)
        return _placeGrowingPlant(blocks, *placed.mGrowingPlant, x, y, z, random);
    if (placed.mSequence != nullptr)
        return _placeSequence(blocks, *placed.mSequence, x, y, z, random);
    if (placed.mSnap != nullptr)
        return _placeSnap(blocks, *placed.mSnap, x, y, z, random);
    if (placed.mBlob != nullptr)
        return _placeBlob(blocks, *placed.mBlob, x, y, z, random);
    if (placed.mMultiface != nullptr)
        return _placeMultiface(blocks, *placed.mMultiface, x, y, z, random);
    if (placed.mDripstoneCluster != nullptr)
        return _placeDripstoneCluster(blocks, *placed.mDripstoneCluster, x, y, z, random);
    if (placed.mPointedDripstone != nullptr)
        return _placePointedDripstone(blocks, *placed.mPointedDripstone, x, y, z, random);
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
    static const BlockStateSet SMALL = {VanillaBlocks::BROWN_MUSHROOM().getIdentifier(),
                                        VanillaBlocks::RED_MUSHROOM().getIdentifier()};
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
            VanillaBlocks::DIRT().getIdentifier(),          VanillaBlocks::COARSE_DIRT().getIdentifier(),
            VanillaBlocks::MYCELIUM().getIdentifier(),      VanillaBlocks::CRIMSON_NYLIUM().getIdentifier(),
            VanillaBlocks::WARPED_NYLIUM().getIdentifier(), VanillaBlocks::PODZOL().getIdentifier(),
            VanillaBlocks::GRASS_BLOCK().getIdentifier(),   VanillaBlocks::DIRT_WITH_ROOTS().getIdentifier()};
    if (!GROUND.contains(blocks.getBlockAt(x, y - 1, z)))
        return false;

    // the cap: a red one hangs three layers of radius 2 below a top of radius 1, a brown one is a single layer of
    // radius 3; huge_mushroom_bits 1 to 9 face the cell's side of the square, 5 the top, 0 the inside
    const BlockState cap =
            (red ? VanillaBlocks::RED_MUSHROOM_BLOCK() : VanillaBlocks::BROWN_MUSHROOM_BLOCK()).toBlockState();
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

bool OverworldRulePlacer::_placeDripstoneCluster(BlockManager &blocks, const DripstoneDefinition &cluster, int32_t x,
                                                 int32_t y, int32_t z, std::mt19937 &random) {
    // sub_1471E6910: density 0.3 to 0.7, radius 2 to 7, 12 blocks searched; wetness drawn even when zero
    if (!isEmptyOrWater(blocks, x, y, z))
        return false;

    const DripstoneBlocks &dripstone = cluster.mBlocks();
    const int32_t height = cluster.mMinHeight + nextInt(random, cluster.mMaxHeight - cluster.mMinHeight + 1);
    float wetness = cluster.mWetness[0] + cluster.mWetness[1] * nextGaussian(random);
    wetness = cluster.mWetness[3] < wetness ? cluster.mWetness[3] : std::fmax(wetness, cluster.mWetness[2]);
    const float densityDraw = nextFloat(random);
    const int32_t radiusX = 2 + nextInt(random, 6);
    const int32_t radiusZ = 2 + nextInt(random, 6);
    const float density = (0.7f - 0.3f) * densityDraw + 0.3f;

    const auto open = [&](int32_t blockX, int32_t blockY, int32_t blockZ) {
        return isEmptyOrWater(blocks, blockX, blockY, blockZ);
    };
    const auto surface = [&](int32_t blockX, int32_t blockY, int32_t blockZ) {
        return isDripstoneBaseOrLava(dripstone, blocks, blockX, blockY, blockZ);
    };
    // a pool may only lie in stone, deepslate, tuff or water
    const auto holdsWater = [&](int32_t blockX, int32_t blockY, int32_t blockZ) {
        static const BlockStateSet WALLS = {"minecraft:stone", "minecraft:deepslate", "minecraft:tuff"};
        const BlockState &state = blocks.getBlockAt(blockX, blockY, blockZ);
        return WALLS.contains(state) || isWater(state) || isWater(blocks.getExtraBlockAt(blockX, blockY, blockZ));
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
                if (!isDripstoneBase(dripstone, floorState) && !isPointedDripstone(dripstone, floorState)
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
                    if (!placeDripstoneBlock(dripstone, blocks, columnX, ceilingY + layer, columnZ))
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
                    if (!placeDripstoneBlock(dripstone, blocks, columnX, floorY - layer, columnZ))
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
                growPointedDripstone(dripstone, blocks, columnX, ceilingY - 1, columnZ, false, stalactite, merge);
            if (floor)
                growPointedDripstone(dripstone, blocks, columnX, floorY + 1, columnZ, true, stalagmite, merge);
        }
    }
    return true;
}

bool OverworldRulePlacer::_placePointedDripstone(BlockManager &blocks, const DripstoneDefinition &pointed, int32_t x,
                                                 int32_t y, int32_t z, std::mt19937 &random) {
    // the game's pointed dripstone (sub_1471E82A0): grows from the base below or above, a patch of dripstone blocks
    // around that base, then one or two blocks long
    const DripstoneBlocks &dripstone = pointed.mBlocks();
    const auto isBase = [&](int32_t blockY) {
        const BlockState &state = blocks.getBlockAt(x, blockY, z);
        return dripstone.mReplaceable.contains(state) || isDripstoneBase(dripstone, state);
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
    placeDripstoneBlock(dripstone, blocks, x, baseY, z);
    // down, up, north, south, west then east, each spreading up to two more blocks in drawn faces
    for (const int32_t *step: FACE_STEPS) {
        if (nextFloat(random) < 0.3f)
            continue;

        int32_t spreadX = x + step[0];
        int32_t spreadY = baseY + step[1];
        int32_t spreadZ = z + step[2];
        placeDripstoneBlock(dripstone, blocks, spreadX, spreadY, spreadZ);
        for (int32_t further = 0; further < 2; further++) {
            if (nextBoolean(random))
                break;

            const int32_t *drawn = FACE_STEPS[nextInt(random, 6)];
            spreadX += drawn[0];
            spreadY += drawn[1];
            spreadZ += drawn[2];
            placeDripstoneBlock(dripstone, blocks, spreadX, spreadY, spreadZ);
        }
    }

    const float taller = nextFloat(random);
    const int32_t nextY = up ? y + 1 : y - 1;
    const int32_t length = isEmptyOrWater(blocks, x, nextY, z) && taller < 0.2f ? 2 : 1;
    growPointedDripstone(dripstone, blocks, x, y, z, up, length, false);
    return true;
}

bool OverworldRulePlacer::_placeSulfurPool(BlockManager &blocks, int32_t x, int32_t y, int32_t z,
                                           std::mt19937 &random) {
    // lake feature sub_14D21A1F0 (sulfur_pool, sub_14D219DA0); legacy walk down the corner column, stops at y 0
    if (y <= LevelChunk::MIN_Y + 4)
        return false;

    const int32_t cornerX = x - 8;
    const int32_t cornerZ = z - 8;
    while (y > LevelChunk::MIN_Y && y > 0 && DecorationSupport::isAir(blocks.getBlockAt(cornerX, y, cornerZ))) {
        y--;
        if (y <= LevelChunk::MIN_Y)
            break;
    }
    const int32_t cornerY = y - 4;

    // x * 128 + z * 8 + y; ellipsoids in floats
    bool lake[2048] = {};
    const int32_t ellipsoids = nextInt(random, 4) + 4;
    for (int32_t i = 0; i < ellipsoids; i++) {
        const float sizeX = nextFloat(random) * 6.0f + 3.0f;
        const float sizeY = nextFloat(random) * 4.0f + 2.0f;
        const float sizeZ = nextFloat(random) * 6.0f + 3.0f;
        const float centerX = ((16.0f - sizeX) + -2.0f) * nextFloat(random) + 1.0f + sizeX * 0.5f;
        const float centerY = (((8.0f - sizeY) + -4.0f) * nextFloat(random) + 2.0f) + sizeY * 0.5f;
        const float centerZ = (((16.0f - sizeZ) + -2.0f) * nextFloat(random) + 1.0f) + sizeZ * 0.5f;
        for (int32_t lakeX = 1; lakeX < 15; lakeX++) {
            const float distanceX = (static_cast<float>(lakeX) - centerX) / (sizeX * 0.5f);
            for (int32_t lakeZ = 1; lakeZ < 15; lakeZ++) {
                const float distanceZ = (static_cast<float>(lakeZ) - centerZ) / (sizeZ * 0.5f);
                for (int32_t lakeY = 1; lakeY < 7; lakeY++) {
                    const float distanceY = (static_cast<float>(lakeY) - centerY) / (sizeY * 0.5f);
                    if (distanceY * distanceY + distanceX * distanceX + distanceZ * distanceZ < 1.0f)
                        lake[lakeX * 128 + lakeZ * 8 + lakeY] = true;
                }
            }
        }
    }

    const auto rim = [&](int32_t lakeX, int32_t lakeY, int32_t lakeZ) {
        const int32_t index = lakeX * 128 + lakeZ * 8 + lakeY;
        return !lake[index]
               && ((lakeX < 15 && lake[index + 128]) || (lakeX > 0 && lake[index - 128])
                   || (lakeZ < 15 && lake[index + 8]) || (lakeZ > 0 && lake[index - 8])
                   || (lakeY < 7 && lake[index + 1]) || (lakeY > 0 && lake[index - 1]));
    };

    for (int32_t lakeX = 0; lakeX < 16; lakeX++) {
        for (int32_t lakeZ = 0; lakeZ < 16; lakeZ++) {
            for (int32_t lakeY = 0; lakeY < 8; lakeY++) {
                if (!rim(lakeX, lakeY, lakeZ))
                    continue;

                const BlockState &state = blocks.getBlockAt(cornerX + lakeX, cornerY + lakeY, cornerZ + lakeZ);
                if (lakeY >= 4 && DecorationSupport::isLiquid(state))
                    return false;
                if (lakeY < 4 && !BlockSupport::isSolid(state) && state.mName != "minecraft:water")
                    return false;
                if (state.mName == "minecraft:sulfur_spike")
                    return false;
            }
        }
    }

    const BlockState water = VanillaBlocks::WATER().toBlockState();
    for (int32_t lakeX = 0; lakeX < 16; lakeX++) {
        for (int32_t lakeZ = 0; lakeZ < 16; lakeZ++) {
            for (int32_t lakeY = 0; lakeY < 8; lakeY++) {
                if (lake[lakeX * 128 + lakeZ * 8 + lakeY])
                    blocks.setBlockStateAt(cornerX + lakeX, cornerY + lakeY, cornerZ + lakeZ,
                                           lakeY < 4 ? water : DecorationSupport::airState());
            }
        }
    }

    // grass over sky-lit dirt skipped: no light during generation, no dirt left in sulfur caves
    const BlockState sulfur = VanillaBlocks::SULFUR().toBlockState();
    for (int32_t lakeX = 0; lakeX < 16; lakeX++) {
        for (int32_t lakeZ = 0; lakeZ < 16; lakeZ++) {
            for (int32_t lakeY = 0; lakeY < 8; lakeY++) {
                if (!rim(lakeX, lakeY, lakeZ) || (lakeY >= 4 && nextInt(random, 2) == 0))
                    continue;

                const int32_t blockX = cornerX + lakeX;
                const int32_t blockY = cornerY + lakeY;
                const int32_t blockZ = cornerZ + lakeZ;
                if (BlockSupport::isSolid(blocks.getBlockAt(blockX, blockY, blockZ)))
                    blocks.setBlockStateAt(blockX, blockY, blockZ, sulfur);
            }
        }
    }
    return true;
}

bool OverworldRulePlacer::_placePotentSulfurSearch(BlockManager &blocks, int32_t x, int32_t y, int32_t z,
                                                   std::mt19937 &random) {
    // search_feature -y (sub_14A261680): y descending, z ascending, x descending; each try draws the single
    // block's weight first (sub_14A267E10)
    static const BlockState wet = [] {
        const BlockState state = VanillaBlocks::POTENT_SULFUR().toBlockState();
        Tag states = state.mStates;
        states.putString("potent_sulfur_state", "wet");
        return BlockState(state.mName, states);
    }();

    for (int32_t offsetY = 0; offsetY >= -8; offsetY--) {
        for (int32_t offsetZ = -8; offsetZ <= 8; offsetZ++) {
            for (int32_t offsetX = 8; offsetX >= -8; offsetX--) {
                nextInt(random, 1);
                const int32_t blockX = x + offsetX;
                const int32_t blockY = y + offsetY;
                const int32_t blockZ = z + offsetZ;
                if (blocks.getBlockAt(blockX, blockY + 1, blockZ).mName != "minecraft:water"
                    || blocks.getBlockAt(blockX, blockY, blockZ).mName != "minecraft:sulfur")
                    continue;

                blocks.setBlockStateAt(blockX, blockY, blockZ, wet);
                return true;
            }
        }
    }
    return false;
}

bool OverworldRulePlacer::_placeLargeDripstone(BlockManager &blocks, int32_t x, int32_t y, int32_t z,
                                               std::mt19937 &random) {
    // the game's large dripstone (sub_14D21B810, vtable 0x14F4A1470): radius 3 to 18, at most a third of the cave;
    // bluntness 0.3..0.9 up, 0.4..1 down; height scale 0.4..2; wind 0..0.3 from radius 4 and bluntness 0.6
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
        return isDripstoneBaseOrLava(dripstoneBlocks(), blocks, blockX, blockY, blockZ);
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
                    static const BlockStateSet SPIKES = {"minecraft:pointed_dripstone", "minecraft:sulfur_spike",
                                                         "minecraft:icicle"};
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
    if (hangingFits && hanging.mY - static_cast<int32_t>(hanging.heightAt(0.0f)) > LevelChunk::MIN_Y) {
        placeBlocks(hanging);
        placed = true;
    }
    if (standingFits && standing.mY + static_cast<int32_t>(standing.heightAt(0.0f)) < LevelChunk::MAX_Y + 1) {
        placeBlocks(standing);
        placed = true;
    }
    return placed;
}

bool OverworldRulePlacer::_placeMultiface(BlockManager &blocks, const MultifaceDefinition &multiface, int32_t x,
                                          int32_t y, int32_t z, std::mt19937 &random) {
    // the game shuffles the faces with a system-seeded thread local random (sub_140391AB0): the first face and
    // the spread can't be reproduced, the faces are tried in their listed order
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

        // every step of the search range offsets the start once: they all try the same block
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
        if (contains(multiface.mCanPlaceOn,
                     blocks.getBlockAt(x + FACE_STEPS[face][0], y + FACE_STEPS[face][1], z + FACE_STEPS[face][2]))) {
            chosen = face;
            break;
        }
    }
    if (chosen < 0)
        return false;

    const BlockState placedState = multiface.mPlaces().toBlockState();
    const BlockState &current = blocks.getBlockAt(x, y, z);
    const int32_t bits = current.mName == placedState.mName ? current.mStates.getInt("multi_face_direction_bits", 0)
                                                            : 0;
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

bool OverworldRulePlacer::_placeVegetationPatch(BlockManager &blocks, const VegetationPatchDefinition &patch, int32_t x,
                                                int32_t y, int32_t z, std::mt19937 &random) {
    // sub_14A282710: two radii, then a disk of columns, corners dropped and edges kept by chance; each column
    // snaps to its own floor and the ground is laid there before any vegetation draws
    const int32_t radiusRange = patch.mRadiusMax - patch.mRadiusMin;
    const auto radius = [&] {
        return patch.mRadiusMin < patch.mRadiusMax - 1 ? patch.mRadiusMin + nextInt(random, radiusRange)
                                                       : patch.mRadiusMin;
    };
    const int32_t radiusX = radius();
    const int32_t radiusZ = radius();

    const int32_t step = patch.mFloor ? -1 : 1;
    const BlockState ground = patch.mGround().toBlockState();

    std::vector<int32_t> columns;
    for (int32_t cx = -radiusX - 1; cx <= radiusX + 1; cx++) {
        const bool edgeX = cx == -radiusX - 1 || cx == radiusX + 1;
        for (int32_t cz = -radiusZ - 1; cz <= radiusZ + 1; cz++) {
            const bool edgeZ = cz == -radiusZ - 1 || cz == radiusZ + 1;
            if (edgeX && edgeZ)
                continue;
            if (edgeX || edgeZ) {
                if (patch.mExtraEdgeChance == 0.0f)
                    continue;
                if (nextFloat(random) > patch.mExtraEdgeChance)
                    continue;
            }

            const int32_t columnX = x + cx;
            const int32_t columnZ = z + cz;
            int32_t columnY = y;
            // the column snap only walks through air: water is ground like any other block
            for (int32_t i = 0;
                 i < patch.mVerticalRange && DecorationSupport::isAir(blocks.getBlockAt(columnX, columnY, columnZ));
                 i++)
                columnY += step;
            for (int32_t j = 0;
                 j < patch.mVerticalRange && !DecorationSupport::isAir(blocks.getBlockAt(columnX, columnY, columnZ));
                 j++)
                columnY -= step;

            if (!DecorationSupport::isAir(blocks.getBlockAt(columnX, columnY, columnZ)))
                continue;

            // a full block stands in for the game's sturdy face test (sub_147A83F50) toward the air; whether the
            // surface may be replaced is only known once the depth is drawn
            const int32_t top = columnY + step;
            if (!DecorationSupport::isFullBlock(blocks.getBlockAt(columnX, top, columnZ)))
                continue;

            int32_t extra = 0;
            if (patch.mExtraDeepChance > 0.0f && patch.mExtraDeepChance > nextFloat(random))
                extra = 1;
            const int32_t depth = (patch.mDepthMin < patch.mDepthMax - 1
                                           ? patch.mDepthMin + nextInt(random, patch.mDepthMax - patch.mDepthMin)
                                           : patch.mDepthMin)
                                  + extra;

            // the ground goes down layer by layer, over the ground block itself without laying it again, and stops at
            // the first block it may not replace; a surface it may not replace keeps no column
            bool kept = true;
            for (int32_t d = 0; d < depth; d++) {
                const int32_t groundY = top + step * d;
                const BlockState &replaced = blocks.getBlockAt(columnX, groundY, columnZ);
                if (replaced == ground)
                    continue;
                if (!contains(patch.mReplaceable, replaced)) {
                    kept = d != 0;
                    break;
                }
                blocks.setBlockStateAt(columnX, groundY, columnZ, ground);
            }
            if (!kept)
                continue;
            columns.push_back(columnX);
            columns.push_back(top);
            columns.push_back(columnZ);
        }
    }

    if (patch.mWaterlogged) {
        // a full block stands in for the game's sturdy face test (sub_147A83F50) on each neighbour's face toward
        // the surface: the four sides and the top of the block below
        std::vector<int32_t> closed;
        for (size_t i = 0; i + 2 < columns.size(); i += 3) {
            const int32_t cx = columns[i];
            const int32_t cy = columns[i + 1];
            const int32_t cz = columns[i + 2];
            if (!DecorationSupport::isFullBlock(blocks.getBlockAt(cx, cy, cz - 1))
                || !DecorationSupport::isFullBlock(blocks.getBlockAt(cx + 1, cy, cz))
                || !DecorationSupport::isFullBlock(blocks.getBlockAt(cx, cy, cz + 1))
                || !DecorationSupport::isFullBlock(blocks.getBlockAt(cx - 1, cy, cz))
                || !DecorationSupport::isFullBlock(blocks.getBlockAt(cx, cy - 1, cz)))
                continue;
            closed.push_back(cx);
            closed.push_back(cy);
            closed.push_back(cz);
        }
        for (size_t i = 0; i + 2 < closed.size(); i += 3)
            blocks.setBlockStateAt(closed[i], closed[i + 1], closed[i + 2], DecorationSupport::waterState());
        columns.swap(closed);
    }

    if (patch.mVegetationChance > 0.0f) {
        int32_t total = 0;
        for (const std::pair<Block (*)(), int32_t> &weighted: patch.mVegetation)
            total += weighted.second;

        for (size_t i = 0; i + 2 < columns.size(); i += 3) {
            if (patch.mVegetationChance <= nextFloat(random))
                continue;

            const int32_t vx = columns[i];
            const int32_t vy = columns[i + 1] - (patch.mWaterlogged ? 1 : 0) - step;
            const int32_t vz = columns[i + 2];
            if (patch.mVegetationFeature.isSet()) {
                _place(blocks, patch.mVegetationFeature, vx, vy, vz, random);
                continue;
            }
            if (patch.mVegetation.empty())
                continue;

            // the moss vegetation (vtable 0x14F2F2630, place sub_14721B570) draws its block before any test
            int32_t roll = (total > 0 ? nextInt(random, total) : 0) - patch.mVegetation.front().second;
            size_t index = 0;
            while (roll >= 0 && index + 1 < patch.mVegetation.size())
                roll -= patch.mVegetation[++index].second;

            if (!DecorationSupport::isAir(blocks.getBlockAt(vx, vy, vz)))
                continue;

            const BlockState plant = patch.mVegetation[index].first().toBlockState();
            if (VanillaBlocks::getAs<DoublePlantBlock>(plant.mName) == nullptr) {
                blocks.setBlockStateAt(vx, vy, vz, plant);
                continue;
            }

            // a double plant (the list at xmmword_1529AB080) gets its upper half too, once DoublePlantBlock::mayPlace
            // has found air above
            if (!DecorationSupport::isAir(blocks.getBlockAt(vx, vy + 1, vz)))
                continue;
            blocks.setBlockStateAt(vx, vy + 1, vz, DecorationSupport::withByteState(plant, "upper_block_bit", true));
            blocks.setBlockStateAt(vx, vy, vz, DecorationSupport::withByteState(plant, "upper_block_bit", false));
        }
    }

    return !columns.empty();
}

bool OverworldRulePlacer::_placeGrowingPlant(BlockManager &blocks, const GrowingPlantDefinition &plant, int32_t x,
                                             int32_t y, int32_t z, std::mt19937 &random) {
    // sub_14A2BEF40: a length drawn from the weighted ranges, an age, then weighted body blocks capped by a
    // weighted head where the next step is not air; a blocked step is skipped, not an end, so the column goes on
    // past it, and a column that never gets its head is a failure even with body blocks laid
    int32_t heightTotal = 0;
    for (const GrowingPlantDefinition::HeightWeight &height: plant.mHeight)
        heightTotal += height.mWeight;

    const GrowingPlantDefinition::HeightWeight *picked = &plant.mHeight.front();
    int32_t heightRoll = (heightTotal > 0 ? nextInt(random, heightTotal) : 0) - plant.mHeight.front().mWeight;
    for (size_t i = 0; heightRoll >= 0 && i + 1 < plant.mHeight.size(); i++) {
        picked = &plant.mHeight[i + 1];
        heightRoll -= picked->mWeight;
    }

    const int32_t length = picked->mMin < picked->mMax - 1 ? picked->mMin + nextInt(random, picked->mMax - picked->mMin)
                                                           : picked->mMin;
    const int32_t age = plant.mAgeMin < plant.mAgeMax - 1
                                ? plant.mAgeMin + nextInt(random, plant.mAgeMax - plant.mAgeMin)
                                : plant.mAgeMin;
    if (length <= 0)
        return false;

    const int32_t step = plant.mDown ? -1 : 1;
    const auto pick = [&](const std::vector<GrowingPlantDefinition::BlockWeight> &blocksWeighted) {
        int32_t total = 0;
        for (const GrowingPlantDefinition::BlockWeight &weighted: blocksWeighted)
            total += weighted.mWeight;
        int32_t roll = (total > 0 ? nextInt(random, total) : 0) - blocksWeighted.front().mWeight;
        size_t index = 0;
        while (roll >= 0 && index + 1 < blocksWeighted.size())
            roll -= blocksWeighted[++index].mWeight;
        return blocksWeighted[index].mState;
    };

    for (int32_t i = 0; i < length; i++) {
        const int32_t columnY = y + step * i;
        const bool open = DecorationSupport::isAir(blocks.getBlockAt(x, columnY, z))
                          || (plant.mAllowWater
                              && (isWater(blocks.getBlockAt(x, columnY, z))
                                  || isWater(blocks.getExtraBlockAt(x, columnY, z))));
        if (!open)
            continue;

        if (i == length - 1 || !DecorationSupport::isAir(blocks.getBlockAt(x, columnY + step, z))) {
            const BlockState &head = pick(plant.mHead);
            setInWater(blocks, x, columnY, z,
                       plant.mAgeMax != 0 ? DecorationSupport::withState(head, "growing_plant_age", age) : head);
            return true;
        }

        setInWater(blocks, x, columnY, z, pick(plant.mBody));
    }

    return false;
}

bool OverworldRulePlacer::_placeDripleaf(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random) {
    // DripleafFeature::place (sub_14D208A20): a small dripleaf when nextFloat < 0.2, else the big dripleaf feature
    // of a direction; both draw the direction with nextInt(4), 0 south, 1 west, 2 north, 3 east
    static const char *const DIRECTIONS[4] = {"south", "west", "north", "east"};
    if (nextFloat(random) < 0.2f) {
        // SmallDripleafBlock placement (sub_148F8FC60): mayPlace (sub_148F8EC40) then the lower half and the upper
        // half above it (sub_148F8F620)
        const BlockState lower = DecorationSupport::withByteState(
                DecorationSupport::withState(VanillaBlocks::SMALL_DRIPLEAF_BLOCK().toBlockState(),
                                             "minecraft:cardinal_direction", DIRECTIONS[nextInt(random, 4)]),
                "upper_block_bit", false);
        if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
            return false;

        const BlockState &at = blocks.getBlockAt(x, y, z);
        const BlockState &above = blocks.getBlockAt(x, y + 1, z);
        if (!BlockSupport::isReplaceable(at) || !BlockSupport::isReplaceable(above))
            return false;

        // mayPlaceOn (sub_148F8EE70): clay or moss anywhere, the dirt-like blocks of xmmword_1529B3258 only under
        // water
        static const BlockStateSet ANYWHERE = {"minecraft:clay", "minecraft:moss_block"};
        static const BlockStateSet UNDER_WATER = {"minecraft:dirt",       "minecraft:grass_block",
                                                  "minecraft:podzol",     "minecraft:coarse_dirt",
                                                  "minecraft:mycelium",   "minecraft:dirt_with_roots",
                                                  "minecraft:moss_block", "minecraft:pale_moss_block",
                                                  "minecraft:mud",        "minecraft:muddy_mangrove_roots",
                                                  "minecraft:farmland"};
        const BlockState &below = blocks.getBlockAt(x, y - 1, z);
        const bool inWater = isWater(at) || isWater(blocks.getExtraBlockAt(x, y, z));
        if (!ANYWHERE.contains(below) && !(inWater && UNDER_WATER.contains(below)))
            return false;

        setInWater(blocks, x, y, z, lower);
        setInWater(blocks, x, y + 1, z, DecorationSupport::withByteState(lower, "upper_block_bit", true));
        return true;
    }

    // big_dripleaf_<direction>_feature: growing_plant_feature growing up through water as well, 1 + nextInt(5)
    // blocks at weight 2 or 1 block at weight 1, without an age
    static const std::array<GrowingPlantDefinition, 4> BIG_DRIPLEAF = [] {
        const auto part = [](const char *direction, bool head) {
            BlockState state = DecorationSupport::withState(VanillaBlocks::BIG_DRIPLEAF().toBlockState(),
                                                            "minecraft:cardinal_direction", direction);
            state = DecorationSupport::withByteState(state, "big_dripleaf_head", head);
            return DecorationSupport::withState(state, "big_dripleaf_tilt", "none");
        };
        const auto feature = [&](const char *identifier, const char *direction) {
            return GrowingPlantDefinition{identifier,
                                          {{1, 6, 2}, {1, 2, 1}},
                                          false,
                                          0,
                                          0,
                                          {{part(direction, false), 1}},
                                          {{part(direction, true), 1}},
                                          true};
        };
        return std::array<GrowingPlantDefinition, 4>{feature("minecraft:big_dripleaf_south_feature", DIRECTIONS[0]),
                                                     feature("minecraft:big_dripleaf_west_feature", DIRECTIONS[1]),
                                                     feature("minecraft:big_dripleaf_north_feature", DIRECTIONS[2]),
                                                     feature("minecraft:big_dripleaf_east_feature", DIRECTIONS[3])};
    }();
    return _placeGrowingPlant(blocks, BIG_DRIPLEAF[nextInt(random, 4)], x, y, z, random);
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
    const int32_t iterations = iterationsOf(scatter.mChanceNumerator, scatter.mChanceDenominator, scatter.mParsedChance,
                                            scatter.mParsedIterations, variables, random);
    bool placed = false;
    for (int32_t iteration = 0; iteration < iterations; iteration++) {
        int32_t position[3] = {0, 0, 0};
        // the grid counts the iterations left, measured on the jungle trees' cocoa
        auto grid = static_cast<uint32_t>(iterations - 1 - iteration);
        scatterPosition(scatter.mParsedAxes.data(), scatter.mEvalOrder, origin, variables, random, position, &grid);
        placed = _place(blocks, scatter.mPlaces, position[0], position[1], position[2], random) || placed;
    }
    return placed;
}

bool OverworldRulePlacer::_placeSingleBlock(BlockManager &blocks, const SingleBlockDefinition &single, int32_t x,
                                            int32_t y, int32_t z, std::mt19937 &random) {
    // SingleBlockFeature::place (sub_14A267E10): the block is drawn by weight even when it is the only one, then its
    // survival is enforced and only air is replaced; a short grass or fern survives on the blocks of PlantSupport::Dirt
    if (single.mNotOnPodzol && blocks.getBlockAt(x, y - 1, z).mName == VanillaBlocks::PODZOL().getIdentifier())
        return false;
    nextInt(random, 1);
    if (single.mAttachesTo != nullptr)
        return _placeAttached(blocks, single, x, y, z);

    const BlockState &support = blocks.getBlockAt(x, y - 1, z);
    const bool hardenedClay = support.mName == "minecraft:hardened_clay"
                              || (BlockIdentifier::endsWith(support.mName, "_terracotta")
                                  && !BlockIdentifier::endsWith(support.mName, "_glazed_terracotta"));
    const bool supported = IFeature::isSupportDirt(support)
                           || (single.mSandAndClay && (DecorationSupport::isSand(support) || hardenedClay))
                           || (single.mNetherPlant
                               && (support.mName == "minecraft:crimson_nylium"
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

bool OverworldRulePlacer::_placeAttached(BlockManager &blocks, const SingleBlockDefinition &single, int32_t x,
                                         int32_t y, int32_t z) {
    // the may_attach_to sides of sub_14A267E10 (sub_14A269500 on north, east, south then west), the last side
    // found rotating the block (sub_145E96810): the cocoa's direction points at it, 0 south, 1 west, 2 north, 3 east
    static const int32_t SIDES[4][3] = {{0, -1, 2}, {1, 0, 3}, {0, 1, 0}, {-1, 0, 1}};
    const std::string attachesTo = single.mAttachesTo().getIdentifier();
    int32_t direction = -1;
    for (const int32_t *side: SIDES) {
        if (blocks.getBlockAt(x + side[0], y, z + side[1]).mName == attachesTo)
            direction = side[2];
    }
    if (direction < 0)
        return false;

    const BlockState state = single.mBlock().toBlockState();
    Tag states = state.mStates;
    states.putInt("age", single.mAge);
    states.putInt("direction", direction);
    blocks.setBlockStateAt(x, y, z, BlockState(state.mName, states));
    return true;
}

bool OverworldRulePlacer::_placeWeighted(BlockManager &blocks, const WeightedDefinition &weighted, int32_t x, int32_t y,
                                         int32_t z, std::mt19937 &random) {
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

bool OverworldRulePlacer::_placeSeagrass(BlockManager &blocks, const SeagrassDefinition &seagrass, int32_t x, int32_t y,
                                         int32_t z, std::mt19937 &random) {
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
        if (DecorationSupport::isAir(at) || isWater(blocks.getBlockAt(plantX, plantY - 1, plantZ)) || isKelp(at))
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
    // GeodeFeature::place (sub_14D20B390); its leading structure overlap test has no counterpart here
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

        // the block and its liquid: the block itself when the second layer is empty
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
        const BlockState bud =
                geode.mInnerPlacements[(size_t) nextInt(random, (int32_t) geode.mInnerPlacements.size())]()
                        .toBlockState();
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
    // LegacyTreeFeature::place (sub_14D224440): 9 in 10 the density plus up to 2, or one tree at that chance below
    // 1; else the density scaled by a float; only the swamp's seagrass reads the rule's y
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
        else if (trees.mPick == Pick::RedwoodMutated)
            picked = nextInt(random, 3) != 0 ? (nextInt(random, 3) != 0 ? 2 : 1) : 0;
        else if (trees.mPick == Pick::MegaTaiga)
            picked = nextInt(random, 3) != 0 ? (nextInt(random, 3) != 0 ? 3 : 2) : (nextInt(random, 13) != 0 ? 1 : 0);
        else if (trees.mPick == Pick::Jungle)
            picked = nextInt(random, 10) != 0 ? (nextInt(random, 2) != 0 ? (nextInt(random, 3) != 0 ? 3 : 2) : 1) : 0;
        else if (trees.mPick == Pick::JungleEdge)
            picked = nextInt(random, 10) != 0 ? (nextInt(random, 2) != 0 ? 2 : 1) : 0;
        else if (trees.mPick == Pick::Savanna)
            picked = nextInt(random, 5) > 0 ? 0 : 1;
        else if (trees.mPick == Pick::BambooJungle)
            picked = nextInt(random, 20) != 0 ? (nextInt(random, 20) > 2 ? (nextInt(random, 10) > 6 ? 3 : 2) : 1) : 0;

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

bool OverworldRulePlacer::_placeTree(BlockManager &blocks, const TreeDefinition &tree, int32_t x, int32_t y, int32_t z,
                                     std::mt19937 &random, int32_t *placedAt) {
    return OverworldTreePlacer::place(
            blocks, tree.mTree, tree.mFancy, tree.mMega, tree.mAcacia, x, y, z, random,
            [&](int32_t logX, int32_t logY, int32_t logZ) {
                _place(blocks, tree.mLogDecoration, logX, logY, logZ, random);
            },
            placedAt);
}

bool OverworldRulePlacer::_placeSequence(BlockManager &blocks, const SequenceDefinition &sequence, int32_t x, int32_t y,
                                         int32_t z, std::mt19937 &random) {
    // SequenceFeature: each feature from the position the one before handed on, stopping at the first that places
    // nothing, measured on the jungle trees: a failed tree draws no cocoa chance; only the trees move the position
    int32_t position[3] = {x, y, z};
    for (const Placed &feature: sequence.mFeatures) {
        if (feature.mTree == nullptr) {
            if (!_place(blocks, feature, position[0], position[1], position[2], random))
                return false;
            continue;
        }
        int32_t next[3] = {position[0], position[1], position[2]};
        if (!_placeTree(blocks, *feature.mTree, position[0], position[1], position[2], random, next))
            return false;
        std::copy(next, next + 3, position);
    }
    return true;
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
    // each side a beehive_feature drawing its block before checking where it attaches
    static const int32_t EAST[2] = {1, 0};
    static const int32_t SOUTH[2] = {0, 1};
    static const int32_t WEST[2] = {-1, 0};
    static const int32_t *const ORDERS[3][3] = {{EAST, SOUTH, WEST}, {SOUTH, WEST, EAST}, {WEST, EAST, SOUTH}};
    // the may_attach_to blocks of beehive_feature
    static const BlockStateSet TRUNKS = {"minecraft:oak_log", "minecraft:birch_log", "minecraft:mangrove_roots",
                                         "minecraft:cherry_log"};
    static const BlockStateSet LEAVES = {"minecraft:oak_leaves", "minecraft:birch_leaves", "minecraft:mangrove_leaves",
                                         "minecraft:cherry_leaves"};
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
    // LegacySpringsFeature::place (sub_14D21E680), 1.18 terrain: 25 water springs from the bottom up to 192, then
    // 20 lava springs biased low, out of the deep dark; z, then y, then x
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
                "minecraft:stone",      "minecraft:deepslate", "minecraft:tuff",
                "minecraft:calcite",    "minecraft:snow",      "minecraft:powder_snow",
                "minecraft:packed_ice", "minecraft:dirt",      "minecraft:coarse_dirt"};
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
    // the game's iceberg feature (sub_14D215C00, vtable 0x14F4A0F30) at sea level, in floats: round or 1 in 5
    // elliptic, its underwater part, smoothing, then most of the time a cut through it (sub_14D217330)
    static const BlockState air = VanillaBlocks::AIR().toBlockState();
    static const BlockState water = VanillaBlocks::WATER().toBlockState();
    static const BlockState snow = VanillaBlocks::SNOW().toBlockState();
    static const BlockState packedIce = VanillaBlocks::PACKED_ICE().toBlockState();
    static const BlockState blueIce = VanillaBlocks::BLUE_ICE().toBlockState();
    constexpr int32_t SEA_LEVEL = 63;

    static const BlockStateSet ICEBERG = {"minecraft:packed_ice", "minecraft:snow", "minecraft:blue_ice"};
    static const BlockStateSet SNOW_LAYER = {VanillaBlocks::SNOW_LAYER().getIdentifier()};
    const auto isIceberg = [](const BlockState &state) { return ICEBERG.contains(state); };

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
        static const BlockStateSet TAKES_ICEBERG = {"minecraft:air", "minecraft:snow", "minecraft:ice",
                                                    "minecraft:water", "minecraft:flowing_water"};
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
                                                   static_cast<float>(width), static_cast<float>(ellipseDepth), cosine,
                                                   sine);
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
                        ringWidth = ceilOf(static_cast<float>((1.0 - dySquared / static_cast<double>(8.0f * depthF))
                                                              * static_cast<double>(width)));
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
                                    cutSin)
                    >= -7.006492321624085e-46)
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

bool OverworldRulePlacer::_placeBlueIce(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random) {
    // the game's blue ice feature (sub_147174460): under the sea, in or over water and next to packed ice, then 200
    // tries around it keeping one to four blue ice neighbours
    static const BlockState blueIce = VanillaBlocks::BLUE_ICE().toBlockState();
    if (y >= 63)
        return false;
    static const BlockStateSet STILL_WATER = {VanillaBlocks::WATER().getIdentifier()};
    static const BlockStateSet PACKED_ICE = {VanillaBlocks::PACKED_ICE().getIdentifier()};
    static const BlockStateSet BLUE_ICE = {VanillaBlocks::BLUE_ICE().getIdentifier()};
    static const BlockStateSet TAKES_BLUE_ICE = {"minecraft:air", "minecraft:water", "minecraft:packed_ice",
                                                 "minecraft:ice"};
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
    // LegacyForestFoliageFeature::place (vtable 0x14F4A14C0, sub_14D21D0B0), forest type 1: nextInt(5) - 3 groups
    // of one double plant, each tried 5 times at a chunk column, below its heightmap plus 32
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
    // the legacy forest rock feature (vtable 0x14F4A14F0, place sub_14D21D7B0): nextInt(3) rocks, each at a chunk
    // column, z drawn first, from its heightmap; the rule's y is not read
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
    static const BlockStateSet SUPPORT = {VanillaBlocks::GRASS_BLOCK().getIdentifier(),
                                          VanillaBlocks::STONE().getIdentifier(), VanillaBlocks::DIRT().getIdentifier(),
                                          VanillaBlocks::COARSE_DIRT().getIdentifier()};
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
