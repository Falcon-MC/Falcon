#include "World/Level/LevelGen/Overworld/Feature/OverworldTreePlacer.h"

#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/FloweredAzaleaLeavesBlock.h"
#include "World/Level/Block/LeavesBlock.h"
#include "World/Level/Block/PlantBlock.h"
#include "World/Level/Block/SnowLayerBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/VineBlock.h"
#include "World/Level/Block/WaterBlock.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/BlockStateSet.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Feature/FeatureMath.h"
#include "World/Level/LevelGen/Feature/IFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace {
    using FeatureRandom::nextBoolean;
    using FeatureRandom::nextFloat;
    using FeatureRandom::nextInt;

    // the game's ranges draw only when they hold more than one value (sub_1471CC3A0)
    int32_t drawRange(std::mt19937 &random, const OverworldTreePlacer::Range &range) {
        return range.mMin < range.mMax - 1 ? range.mMin + nextInt(random, range.mMax - range.mMin) : range.mMin;
    }

    bool isAir(const BlockState &state) {
        return DecorationSupport::isAir(state);
    }

    bool isLeaves(const BlockState &state) {
        return VanillaBlocks::getAs<LeavesBlock>(state.mName) != nullptr
               || VanillaBlocks::getAs<FloweredAzaleaLeavesBlock>(state.mName) != nullptr;
    }

    // may_replace of the vanilla trees: air, snow layers, the leaves tag, wildflowers and leaf litter
    bool mayReplace(const BlockState &state) {
        static const BlockStateSet GROUND_COVER = BlockStateSet::ofBlocks([](const Block &block) {
            return dynamic_cast<const SnowLayerBlock *>(&block) != nullptr
                   || block.getIdentifier() == VanillaBlocks::WILDFLOWERS().getIdentifier()
                   || block.getIdentifier() == VanillaBlocks::LEAF_LITTER().getIdentifier();
        });
        return isAir(state) || isLeaves(state) || GROUND_COVER.contains(state);
    }

    // may_grow_on of the vanilla trees, the dirt tag
    bool mayGrowOn(const BlockState &state) {
        return IFeature::isSupportDirt(state);
    }

    bool isWater(const BlockState &state) {
        return VanillaBlocks::getAs<WaterBlock>(state.mName) != nullptr;
    }

    // may_replace of the mangroves adds water, vines, moss carpets and propagules
    bool isMangroveCover(const BlockState &state) {
        static const BlockStateSet COVER = {
                VanillaBlocks::VINE().getIdentifier(), VanillaBlocks::MOSS_CARPET().getIdentifier(),
                VanillaBlocks::MANGROVE_PROPAGULE().getIdentifier()
        };
        return isWater(state) || COVER.contains(state);
    }

    bool replaces(const OverworldTreePlacer::Definition &tree, const BlockState &state) {
        if (tree.mAirAndLeaves)
            return isAir(state) || isLeaves(state);
        if (tree.mMangrove != nullptr && isMangroveCover(state))
            return true;
        return mayReplace(state) || (tree.mInWater && isWater(state));
    }

    // roots_may_grow_through, or the air, water, leaves and plants the roots go over (sub_1471C5910)
    bool rootPasses(const BlockState &state) {
        static const BlockStateSet THROUGH = {
                VanillaBlocks::SNOW_LAYER().getIdentifier(), VanillaBlocks::MUD().getIdentifier(),
                VanillaBlocks::MANGROVE_ROOTS().getIdentifier(), VanillaBlocks::MUDDY_MANGROVE_ROOTS().getIdentifier()
        };
        return THROUGH.contains(state) || isMangroveCover(state) || isAir(state) || isLeaves(state)
               || (PlantBlock::matches(state.mName) && BlockSupport::isReplaceable(state));
    }

    // the trunk's room list (+104 of the tree): may_replace and, without water among it, may_grow_through
    bool roomFor(const OverworldTreePlacer::Definition &tree, const BlockState &state) {
        return replaces(tree, state) || (!tree.mInWater && mayGrowOn(state));
    }

    // the leaves of the simple and spruce canopies go into air, leaves (material types 0 and 7) and replaceable
    // plants (type 8), vines among them
    bool canopyReplaces(const BlockState &state) {
        static const BlockStateSet VINES = BlockStateSet::ofBlocks([](const Block &block) {
            return dynamic_cast<const VineBlock *>(&block) != nullptr;
        });
        return isAir(state) || isLeaves(state) || VINES.contains(state)
               || (PlantBlock::matches(state.mName) && BlockSupport::isReplaceable(state));
    }

    // the trunk's base blocks, left as they are under it
    bool isBaseBlock(const BlockState &state) {
        static const BlockStateSet BASE = {
                VanillaBlocks::DIRT().getIdentifier(), VanillaBlocks::COARSE_DIRT().getIdentifier()
        };
        return BASE.contains(state);
    }

    // the vanilla simple canopy: offsets -3 to 0 and a slope of 1 / 2, corners skipped one time in two below the
    // top layer and always on it
    const OverworldTreePlacer::SimpleCanopy &vanillaCanopy(int32_t minWidth) {
        static const std::vector<std::pair<uint32_t, uint32_t>> VARIATION = {{1, 2}, {1, 2}, {1, 2}, {1, 1}};
        static const OverworldTreePlacer::SimpleCanopy WIDTHS[3] = {
                {-3, 0, 0, 0.5f, VARIATION}, {-3, 0, 1, 0.5f, VARIATION}, {-3, 0, 2, 0.5f, VARIATION}
        };
        return WIDTHS[std::min(std::max(minWidth, 0), 2)];
    }

    BlockState withAxis(const BlockState &log, const char *axis) {
        Tag states = log.mStates;
        states.putString("pillar_axis", axis);
        return BlockState(log.mName, states);
    }

    // a log line of the fancy trunk: steps along the longest axis, each offset rounded from the start
    // (sub_14A290E90); a log turns along x or z once it leaves the start
    void logLine(BlockManager &blocks, const BlockState &log, int32_t x, int32_t y, int32_t z, int32_t dx, int32_t dy,
                 int32_t dz) {
        const float steps = std::max(std::max(std::fabs(static_cast<float>(dx)), std::fabs(static_cast<float>(dy))),
                                     std::fabs(static_cast<float>(dz)));
        const auto count = static_cast<int32_t>(steps);
        // 0 / 0 offsets land outside the world in the game
        if (count == 0)
            return;

        const float total = static_cast<float>(count);
        const float stepX = static_cast<float>(dx) / total;
        const float stepY = static_cast<float>(dy) / total;
        const float stepZ = static_cast<float>(dz) / total;
        for (int32_t step = 0; step <= count; step++) {
            const float at = static_cast<float>(step);
            const auto offsetX = static_cast<int32_t>(at * stepX + 0.5f);
            const auto offsetY = static_cast<int32_t>(at * stepY + 0.5f);
            const auto offsetZ = static_cast<int32_t>(stepZ * at + 0.5f);
            const int32_t alongX = std::abs(offsetX);
            const int32_t alongZ = std::abs(offsetZ);
            const char *axis = (alongX | alongZ) == 0 ? "y" : (alongX >= alongZ ? "x" : "z");
            blocks.setBlockStateAt(x + offsetX, y + offsetY, z + offsetZ, withAxis(log, axis));
        }
    }
}

bool OverworldTreePlacer::place(BlockManager &blocks, const Definition &tree, const Fancy *fancy, const Mega *mega,
                                const Acacia *acacia, int32_t x, int32_t y, int32_t z, std::mt19937 &random,
                                const Decorate &decorate, int32_t *placedAt) {
    // VanillaTreeFeature::place (sub_14A2C9A20): the trunk height first, then the trunk, which places the canopy
    if (placedAt != nullptr) {
        placedAt[0] = x;
        placedAt[1] = y;
        placedAt[2] = z;
    }
    if (tree.mTrunk == Trunk::Fancy) {
        // sub_14A291F30
        const int32_t height = fancy->mBase + nextInt(random, fancy->mVariance);
        return _fancyTrunk(blocks, tree, *fancy, x, y, z, height, random);
    }
    if (tree.mTrunk == Trunk::Mega) {
        // sub_1471C87C0: the base plus one draw per interval
        int32_t height = mega->mBaseHeight;
        for (int32_t interval: mega->mHeightIntervals)
            height += nextInt(random, interval);
        return _megaTrunk(blocks, tree, *mega, x, y, z, height, random);
    }
    if (tree.mTrunk == Trunk::Acacia) {
        // sub_14A28E9B0: the base plus one draw per interval
        int32_t height = acacia->mBaseHeight;
        for (int32_t interval: acacia->mHeightIntervals)
            height += nextInt(random, interval);
        return _acaciaTrunk(blocks, tree, *acacia, x, y, z, height, random);
    }

    if (tree.mTrunk == Trunk::Mangrove) {
        // the trunk height first (sub_1471C67E0), then the roots, the trunk standing on them
        const Mangrove &mangrove = *tree.mMangrove;
        const int32_t height = mangrove.mBaseHeight + nextInt(random, mangrove.mHeightRandA + 1)
                               + nextInt(random, mangrove.mHeightRandB + 1);
        int32_t trunkY = y;
        if (!_mangroveRoots(blocks, tree, mangrove, x, y, z, height, random, trunkY))
            return false;
        _mangroveTrunk(blocks, tree, mangrove, x, trunkY, z, height, random);
        return true;
    }

    const int32_t height = drawRange(random, tree.mHeight);
    if (tree.mTrunk == Trunk::Fallen)
        return _fallenTrunk(blocks, tree, x, y, z, height, random, decorate);
    return _simpleTrunk(blocks, tree, x, y, z, height, random, placedAt);
}

bool OverworldTreePlacer::logReplaces(const Definition &tree, const BlockState &state) {
    // sub_1471C66B0: may_replace, else air, water (material 5), leaves (7) and the plants that can be built over
    return replaces(tree, state) || isAir(state) || isWater(state) || canopyReplaces(state);
}

bool OverworldTreePlacer::_canPlace(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                                    int32_t height) {
    // sub_1471C5670: inside the dimension, on may_grow_on, and every block of the trunk's column, its ring above the
    // base and the two rings of 2 at its top replaceable or of the dirt tag the vanilla trees grow through; the
    // game's list holds both, the dirt shown by the trees standing against slopes in its worlds
    if (y <= LevelChunk::MIN_Y || y + height >= LevelChunk::MAX_Y + 1)
        return false;
    if (!mayGrowOn(blocks.getBlockAt(x, y - 1, z)))
        return false;

    for (int32_t at = y; at <= y + height + 1; at++) {
        const int32_t radius = at >= y + height - 1 ? 2 : (at == y ? 0 : 1);
        for (int32_t offsetX = -radius; offsetX <= radius; offsetX++) {
            for (int32_t offsetZ = -radius; offsetZ <= radius; offsetZ++) {
                const BlockState &state = blocks.getBlockAt(x + offsetX, at, z + offsetZ);
                if (!roomFor(tree, state))
                    return false;
            }
        }
    }
    return true;
}

bool OverworldTreePlacer::_simpleTrunk(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                                       int32_t height, std::mt19937 &random, int32_t *placedAt) {
    // SimpleTreeTrunk::place (sub_1471CBEA0): first down through up to max_depth replaceable blocks
    for (int32_t depth = 0; depth < tree.mMaxDepth && replaces(tree, blocks.getBlockAt(x, y - 1, z)); depth++)
        y--;
    if (!_canPlace(blocks, tree, x, y, z, height))
        return false;

    const int32_t total = height + drawRange(random, tree.mHeightModifier);
    if (total <= 0)
        return false;

    const BlockState log = tree.mLog().toBlockState();
    bool placed = false;
    int32_t top = y;
    for (int32_t step = 0; step < total; step++) {
        if (!replaces(tree, blocks.getBlockAt(x, y + step, z)))
            continue;

        blocks.setBlockStateAt(x, y + step, z, log);
        placed = true;
        top = y + step;
        _vines(blocks, tree, x, top, z, random);
    }
    if (!placed)
        return false;

    if (!_canopy(blocks, tree, x, top + 1, z, random))
        return false;

    // the block under the trunk becomes the first base block unless it is one already
    const BlockState &below = blocks.getBlockAt(x, y - 1, z);
    if (!isBaseBlock(below))
        blocks.setBlockStateAt(x, y - 1, z, VanillaBlocks::DIRT().toBlockState());
    // the feature hands on one above the last log, whatever the canopy placed
    if (placedAt != nullptr)
        placedAt[1] = top + 1;
    return true;
}

bool OverworldTreePlacer::_fancyTrunk(BlockManager &blocks, const Definition &tree, const Fancy &fancy, int32_t x,
                                      int32_t y, int32_t z, int32_t height, std::mt19937 &random) {
    // FancyTreeTrunk::place (sub_14A290E90); its line checks always pass: the vanilla trees leave their list empty
    if (!mayGrowOn(blocks.getBlockAt(x, y - 1, z)))
        return false;

    const float size = static_cast<float>(height);
    const int32_t scaled = static_cast<int32_t>(fancy.mScale * size);
    const int32_t trunkHeight = scaled < height - 1 ? scaled : height - 1;
    const int32_t spread = fancy.mVariance + 1;
    const float perLayer = (fancy.mWidthScale * size) / static_cast<float>(spread);
    const int32_t clustersPerLayer = std::max(1, static_cast<int32_t>(perLayer * perLayer + 1.382f));
    const int32_t trunkTop = y + trunkHeight;
    const int32_t foliageHeight = spread / 3;

    struct Cluster {
        int32_t mX;
        int32_t mY;
        int32_t mZ;
        int32_t mBranchY;
    };
    std::vector<Cluster> clusters = {{x, y + height - foliageHeight, z, trunkTop}};

    const float half = 0.5f * size;
    const float halfSquared = half * half;
    for (int32_t layer = height - foliageHeight; layer >= 0; layer--) {
        const auto at = static_cast<float>(layer);
        if (!(fancy.mFoliageAltitudeFactor * size <= at))
            continue;

        float radius = half;
        const float offset = half - at;
        if (offset != 0.0f || std::isnan(offset)) {
            radius = 0.0f;
            if (half > std::fabs(offset))
                radius = std::sqrt(halfSquared - offset * offset);
        }
        radius *= 0.5f;
        if (!(radius >= 0.0f))
            continue;

        for (int32_t cluster = 0; cluster < clustersPerLayer; cluster++) {
            const float reach = (nextFloat(random) + 0.328f) * (fancy.mBranchDensity * radius);
            const float turn = nextFloat(random);
            const float angle = ((turn + turn) * 3.1415927f) * 10430.378f;
            const int32_t clusterX = x + static_cast<int32_t>(reach * FeatureMath::sineAt(angle) + 0.5f);
            const int32_t clusterZ = z + static_cast<int32_t>(FeatureMath::sineAt(angle + 16384.0f) * reach + 0.5f);
            const int32_t clusterY = y + layer - 1;

            const int32_t awayX = x - clusterX;
            const int32_t awayZ = z - clusterZ;
            const auto distance = static_cast<int32_t>(std::sqrt(static_cast<double>(awayX * awayX + awayZ * awayZ)));
            const float branch = static_cast<float>(clusterY)
                                 - static_cast<float>(distance) * fancy.mBranchSlope;
            const int32_t branchY = branch > static_cast<float>(trunkTop) ? trunkTop : static_cast<int32_t>(branch);
            clusters.push_back({clusterX, clusterY, clusterZ, branchY});
        }
    }

    // FancyTreeCanopy::place (sub_14A290B00): the leaves of every cluster, then the trunk and the branches over them
    for (const Cluster &cluster: clusters) {
        for (int32_t layer = 0; layer < fancy.mCanopyHeight; layer++) {
            const int32_t radius = layer == 0 ? fancy.mCanopyRadius - 1
                                              : fancy.mCanopyRadius - (layer == fancy.mCanopyHeight - 1 ? 1 : 0);
            _fancyLayer(blocks, tree, cluster.mX, cluster.mY + layer, cluster.mZ, radius);
        }
    }

    const BlockState log = tree.mLog().toBlockState();
    for (int32_t offsetX = 0; offsetX < fancy.mTrunkWidth; offsetX++) {
        for (int32_t offsetZ = 0; offsetZ < fancy.mTrunkWidth; offsetZ++)
            logLine(blocks, log, x + offsetX, y, z + offsetZ, 0, trunkHeight, 0);
    }

    for (const Cluster &cluster: clusters) {
        if (!(static_cast<float>(cluster.mBranchY - y) >= fancy.mMinAltitudeFactor * size))
            continue;

        const int32_t baseX = cluster.mX <= x ? x : x + fancy.mTrunkWidth - 1;
        const int32_t baseZ = cluster.mZ <= z ? z : z + fancy.mTrunkWidth - 1;
        logLine(blocks, log, baseX, cluster.mBranchY, baseZ, cluster.mX - baseX, cluster.mY - cluster.mBranchY,
                cluster.mZ - baseZ);
    }
    return true;
}

void OverworldTreePlacer::_fancyLayer(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                                      int32_t radius) {
    // sub_14A290C50: a disc whose cells are within radius from their far corner
    const BlockState leaves = tree.mLeaves().toBlockState();
    const auto limit = static_cast<double>(radius * radius);
    for (int32_t offsetX = -radius; offsetX <= radius; offsetX++) {
        const float cornerX = static_cast<float>(std::abs(offsetX)) + 0.5f;
        const float squaredX = cornerX * cornerX;
        for (int32_t offsetZ = -radius; offsetZ <= radius; offsetZ++) {
            const float cornerZ = static_cast<float>(std::abs(offsetZ)) + 0.5f;
            if (!(limit >= static_cast<double>(cornerZ * cornerZ) + static_cast<double>(squaredX)))
                continue;
            if (!mayReplace(blocks.getBlockAt(x + offsetX, y, z + offsetZ)))
                continue;

            blocks.setBlockStateAt(x + offsetX, y, z + offsetZ, leaves);
        }
    }
}

bool OverworldTreePlacer::_fallenTrunk(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                                       int32_t length, std::mt19937 &random, const Decorate &decorate) {
    // FallenTreeTrunk::place (sub_14A2903A0)
    if (!_canPlace(blocks, tree, x, y, z, length))
        return false;

    const int32_t modifier = drawRange(random, tree.mHeightModifier);
    // a horizontal direction (2 north, 3 south, 4 west, 5 east, the table at xmmword_14F166AC4) and the gap to
    // the stump
    static const int32_t DIRECTIONS[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
    const int32_t direction = nextInt(random, 4);
    const int32_t gap = nextInt(random, 2) + 2;
    const int32_t stepX = DIRECTIONS[direction][0];
    const int32_t stepZ = DIRECTIONS[direction][1];
    const int32_t startX = x + stepX * gap;
    const int32_t startZ = z + stepZ * gap;
    // the stump height, 1 for every vanilla fallen tree
    const int32_t stump = 1;

    int32_t startY = y + stump;
    while (startY > LevelChunk::MIN_Y) {
        const BlockState &below = blocks.getBlockAt(startX, startY - 1, startZ);
        if (!isAir(below) && !BlockSupport::isReplaceable(below))
            break;
        startY--;
    }

    const int32_t logs = length + modifier - 2;
    bool fits = true;
    int32_t unsupported = 0;
    for (int32_t step = 0; step < logs; step++) {
        const int32_t logX = startX + stepX * step;
        const int32_t logZ = startZ + stepZ * step;
        const BlockState &at = blocks.getBlockAt(logX, startY, logZ);
        if (!isAir(at) && !BlockSupport::isReplaceable(at))
            fits = false;

        const bool supported = BlockSupport::isSolid(blocks.getBlockAt(logX, startY - 1, logZ));
        fits = fits && (supported || unsupported < 2);
        unsupported = supported ? 0 : unsupported + 1;
    }

    const BlockState log = tree.mLog().toBlockState();
    if (fits) {
        const BlockState lying = withAxis(log, stepX != 0 ? "x" : "z");
        for (int32_t step = 0; step < logs; step++) {
            const int32_t logX = startX + stepX * step;
            const int32_t logZ = startZ + stepZ * step;
            blocks.setBlockStateAt(logX, startY, logZ, lying);
            if (tree.mLogDecoration)
                decorate(logX, startY, logZ);
        }
    }

    bool placed = false;
    for (int32_t step = 0; step < stump; step++) {
        if (!mayReplace(blocks.getBlockAt(x, y + step, z))) {
            placed = false;
            continue;
        }

        blocks.setBlockStateAt(x, y + step, z, log);
        _vines(blocks, tree, x, y + step, z, random);
        placed = true;
    }

    const BlockState &below = blocks.getBlockAt(x, y - 1, z);
    if (!isBaseBlock(below))
        blocks.setBlockStateAt(x, y - 1, z, VanillaBlocks::DIRT().toBlockState());
    return placed;
}

bool OverworldTreePlacer::_megaTrunk(BlockManager &blocks, const Definition &tree, const Mega &mega, int32_t x,
                                     int32_t y, int32_t z, int32_t height, std::mt19937 &random) {
    // MegaTreeTrunk::place (sub_1471C7140)
    if (y <= LevelChunk::MIN_Y || y + height >= LevelChunk::MAX_Y + 1)
        return false;
    if (!mayGrowOn(blocks.getBlockAt(x, y - 1, z)))
        return false;

    // room for the trunk: a ring of max(|width - 1|, 1) on the ground layer and of the width up to two above it,
    // each block replaceable or one the trunk grows through
    const int32_t width = mega.mTrunkWidth;
    for (int32_t offsetY = 0; offsetY < height + 2; offsetY++) {
        const int32_t radius = offsetY == 0 ? std::max(std::abs(width - 1), 1) : width;
        const int32_t at = y + offsetY;
        if (at < LevelChunk::MIN_Y || at >= LevelChunk::MAX_Y + 1)
            return false;
        for (int32_t offsetX = -radius; offsetX <= radius; offsetX++) {
            for (int32_t offsetZ = -radius; offsetZ <= radius; offsetZ++) {
                const BlockState &state = blocks.getBlockAt(x + offsetX, at, z + offsetZ);
                if (!mayReplace(state) && !mayGrowOn(state))
                    return false;
            }
        }
    }

    // the canopy goes first, from the top of the trunk, then the branches
    bool placed = tree.mCanopy == Canopy::Mega
                  ? _megaCanopy(blocks, tree, mega.mCanopy, width, x, y + height, z, random)
                  : _megaPineCanopy(blocks, tree, mega, x, y + height, z, random);
    if (mega.mBranchLength > 0 && _megaBranches(blocks, tree, mega, x, y, z, height, random))
        placed = true;

    const BlockState log = tree.mLog().toBlockState();
    const auto placeLog = [&](int32_t logX, int32_t logY, int32_t logZ) {
        if (!mayReplace(blocks.getBlockAt(logX, logY, logZ)))
            return false;
        blocks.setBlockStateAt(logX, logY, logZ, log);
        placed = true;
        return true;
    };
    // the ground layer, the layers up to two below the top with their vines on the outer sides, then one log at
    // the top corner
    for (int32_t offsetX = 0; offsetX < width; offsetX++) {
        for (int32_t offsetZ = 0; offsetZ < width; offsetZ++)
            placeLog(x + offsetX, y, z + offsetZ);
    }
    for (int32_t offsetX = 0; offsetX < width; offsetX++) {
        for (int32_t at = y + 1; at <= y + height - 2; at++) {
            for (int32_t offsetZ = 0; offsetZ < width; offsetZ++) {
                if (!placeLog(x + offsetX, at, z + offsetZ))
                    continue;

                const bool sides[4] = {offsetX == 0, offsetX != 0 && offsetX == width - 1, offsetZ == 0,
                                       offsetZ != 0 && offsetZ == width - 1};
                _decorateLog(blocks, mega.mVineNumerator, mega.mVineDenominator, x + offsetX, at, z + offsetZ,
                             sides, random);
            }
        }
    }
    placeLog(x, y + height - 1, z);

    // a dirt base stands for the vanilla list, dirt then coarse dirt
    const BlockState base = mega.mBaseBlock().toBlockState();
    const bool dirtBase = base.mName == VanillaBlocks::DIRT().getIdentifier();
    for (int32_t offsetX = 0; offsetX < width; offsetX++) {
        for (int32_t offsetZ = 0; offsetZ < width; offsetZ++) {
            const BlockState &below = blocks.getBlockAt(x + offsetX, y - 1, z + offsetZ);
            if (below.mName != base.mName && !(dirtBase && isBaseBlock(below)))
                blocks.setBlockStateAt(x + offsetX, y - 1, z + offsetZ, base);
        }
    }

    if (mega.mClusters <= 0)
        return placed;

    // a disc around each corner of the trunk, then around cells drawn on the border of the 8 by 8 square centred
    // on it
    const auto cluster = [&](int32_t centreX, int32_t centreZ) {
        const int32_t radius = mega.mClusterRadius;
        for (int32_t offsetX = -radius; offsetX <= radius; offsetX++) {
            for (int32_t offsetZ = -radius; offsetZ <= radius; offsetZ++) {
                if (std::abs(offsetX) != radius || std::abs(offsetZ) != radius)
                    _baseCluster(blocks, mega, centreX + offsetX, y - 1, centreZ + offsetZ);
            }
        }
    };
    cluster(x - 1, z - 1);
    cluster(x + width, z - 1);
    cluster(x - 1, z + width);
    cluster(x + width, z + width);
    for (int32_t drawn = 0; drawn < mega.mClusters; drawn++) {
        const int32_t cell = nextInt(random, 64);
        const int32_t cellX = cell % 8;
        const int32_t cellZ = cell / 8;
        if (cellX == 0 || cellX == 7 || cellZ == 0 || cellZ == 7)
            cluster(x - 3 + cellX, z - 3 + cellZ);
    }
    return placed;
}

bool OverworldTreePlacer::_megaBranches(BlockManager &blocks, const Definition &tree, const Mega &mega, int32_t x,
                                        int32_t y, int32_t z, int32_t height, std::mt19937 &random) {
    // the branch loop of sub_1471C7140: from the altitude's top down by intervals, each a turn drawn, a sloped line
    // of logs set over whatever is there and a canopy at the branch height above its last log
    const auto size = static_cast<float>(height);
    const int32_t lowest = y + static_cast<int32_t>(mega.mBranchAltitudeMin * size);
    int32_t branchY = static_cast<int32_t>(mega.mBranchAltitudeMax * size) + y - drawRange(random, mega.mBranchInterval);
    const BlockState log = tree.mLog().toBlockState();
    bool placed = false;
    while (branchY > lowest) {
        const float turn = nextFloat(random);
        const float angle = (turn * 3.1415927f + turn * 3.1415927f) * 10430.378f;
        const int32_t length = mega.mBranchLength;
        const auto start = static_cast<float>(static_cast<int32_t>(
                static_cast<float>(branchY - 1) - static_cast<float>(length - 1) * mega.mBranchSlope));
        int32_t endX = x + 1;
        int32_t endZ = z + 1;
        for (int32_t step = 0; step < length; step++) {
            const auto at = static_cast<float>(step);
            endX = x + static_cast<int32_t>(FeatureMath::sineAt(angle + 16384.0f) * at + 1.5f);
            endZ = z + static_cast<int32_t>(FeatureMath::sineAt(angle) * at + 1.5f);
            blocks.setBlockStateAt(endX, static_cast<int32_t>(at * mega.mBranchSlope + start), endZ, log);
            placed = true;
        }
        _megaCanopy(blocks, tree, mega.mBranchCanopy, 1, endX, branchY, endZ, random);
        branchY -= drawRange(random, mega.mBranchInterval);
    }
    return placed;
}

bool OverworldTreePlacer::_megaCanopy(BlockManager &blocks, const Definition &tree, const MegaCanopy &canopy,
                                      int32_t width, int32_t x, int32_t y, int32_t z, std::mt19937 &random) {
    // MegaTreeCanopy::place (sub_1471C6C40): bottom up to the position, each layer one wider than the one above;
    // without core_width the radius counts a core of 1 (xmmword_14F2F23D0) but the square spans the trunk's width
    const int32_t height = drawRange(random, canopy.mHeight);
    if (height == 0)
        return false;

    const int32_t core = canopy.mCoreWidth > 0 ? canopy.mCoreWidth : 1;
    const int32_t span = canopy.mCoreWidth > 0 ? canopy.mCoreWidth : width;
    const BlockState leaves = tree.mLeaves().toBlockState();
    bool placed = false;
    for (int32_t offsetY = 1 - height; offsetY <= 0; offsetY++) {
        const int32_t radius = core / 2 + canopy.mBaseRadius - offsetY;
        const int32_t limit = radius * radius;
        for (int32_t offsetX = -radius; offsetX < span + radius; offsetX++) {
            for (int32_t offsetZ = -radius; offsetZ < span + radius; offsetZ++) {
                const int32_t alongX = offsetX * offsetX;
                const int32_t besideX = (offsetX - 1) * (offsetX - 1);
                const int32_t alongZ = offsetZ * offsetZ;
                const int32_t besideZ = (offsetZ - 1) * (offsetZ - 1);
                const bool inside = canopy.mSimplify
                                    ? alongX + alongZ <= limit
                                    : alongZ + alongX <= limit || besideZ + besideX <= limit
                                      || alongX + besideZ <= limit || besideX + alongZ <= limit;
                if (!inside || !mayReplace(blocks.getBlockAt(x + offsetX, y + offsetY, z + offsetZ)))
                    continue;

                blocks.setBlockStateAt(x + offsetX, y + offsetY, z + offsetZ, leaves);
                placed = true;
            }
        }
    }
    return placed;
}

void OverworldTreePlacer::_baseCluster(BlockManager &blocks, const Mega &mega, int32_t x, int32_t y, int32_t z) {
    // sub_1471CC850: from two above down to three below, the first block of the dirt tag becomes the base block,
    // and a base block already there ends the search
    const BlockState base = mega.mBaseBlock().toBlockState();
    for (int32_t offsetY = 2; offsetY >= -3; offsetY--) {
        const BlockState &state = blocks.getBlockAt(x, y + offsetY, z);
        if (state.mName == base.mName)
            return;
        if (!mayGrowOn(state))
            continue;

        blocks.setBlockStateAt(x, y + offsetY, z, base);
        return;
    }
}

bool OverworldTreePlacer::_megaPineCanopy(BlockManager &blocks, const Definition &tree, const Mega &mega, int32_t x,
                                          int32_t y, int32_t z, std::mt19937 &random) {
    // MegaPineCanopy::place (sub_1471C6830): bottom up to the trunk's top, a disc around the core, its radius
    // growing with the depth below the top, widened by one on even layers keeping the previous radius
    const int32_t height = drawRange(random, mega.mCanopyHeight);
    const int32_t core = mega.mCoreWidth;
    const BlockState leaves = tree.mLeaves().toBlockState();
    bool placed = false;
    int32_t previous = 0;
    for (int32_t leafY = y - height; leafY <= y; leafY++) {
        const int32_t depth = y - leafY;
        const int32_t grown = mega.mBaseRadius
                              + static_cast<int32_t>(std::floor(static_cast<float>(depth) / static_cast<float>(height)
                                                                * mega.mRadiusStepModifier));
        const bool same = grown == previous;
        previous = grown;
        const int32_t radius = grown + (same && depth > 0 && (leafY & 1) == 0 ? 1 : 0);
        const int32_t limit = radius * radius;
        for (int32_t offsetX = -radius; offsetX < core + radius; offsetX++) {
            for (int32_t offsetZ = -radius; offsetZ < core + radius; offsetZ++) {
                const int32_t alongX = offsetX * offsetX;
                const int32_t besideX = (offsetX - 1) * (offsetX - 1);
                const int32_t alongZ = offsetZ * offsetZ;
                const int32_t besideZ = (offsetZ - 1) * (offsetZ - 1);
                if (alongZ + alongX > limit && besideZ + besideX > limit && alongX + besideZ > limit
                    && besideX + alongZ > limit)
                    continue;
                if (!mayReplace(blocks.getBlockAt(x + offsetX, leafY, z + offsetZ)))
                    continue;

                blocks.setBlockStateAt(x + offsetX, leafY, z + offsetZ, leaves);
                placed = true;
            }
        }
    }
    return placed;
}

bool OverworldTreePlacer::_canopy(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                                  std::mt19937 &random) {
    switch (tree.mCanopy) {
        case Canopy::Simple:
            return _simpleCanopy(blocks, tree, x, y, z, random);
        case Canopy::Spruce:
            return _spruceCanopy(blocks, tree, x, y, z, random);
        case Canopy::Pine:
            return _pineCanopy(blocks, tree, x, y, z, random);
        default:
            return false;
    }
}

bool OverworldTreePlacer::_simpleCanopy(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                                        std::mt19937 &random) {
    // SimpleTreeCanopy::place (sub_1471CB0F0): each layer's radius from its offset and the slope; a corner, the
    // centre of a layer of radius 0 among them, is skipped at its layer's variation chance
    const SimpleCanopy &shape = tree.mSimpleCanopy != nullptr ? *tree.mSimpleCanopy : vanillaCanopy(tree.mCanopyMinWidth);
    const BlockState leaves = tree.mLeaves().toBlockState();
    const int32_t base = shape.mMinWidth + static_cast<int32_t>(static_cast<float>(shape.mMaxOffset) * shape.mSlope);
    bool placed = false;
    for (int32_t offsetY = shape.mMinOffset; offsetY <= shape.mMaxOffset; offsetY++) {
        const int32_t radius = base - static_cast<int32_t>(static_cast<float>(offsetY) * shape.mSlope);
        for (int32_t offsetX = -radius; offsetX <= radius; offsetX++) {
            for (int32_t offsetZ = -radius; offsetZ <= radius; offsetZ++) {
                if (std::abs(offsetX) == radius && std::abs(offsetZ) == radius) {
                    const std::pair<uint32_t, uint32_t> &chance = shape.mVariation[(size_t) (offsetY - shape.mMinOffset)];
                    if (chance.second != 0 && (chance.first == chance.second || random() % chance.second < chance.first))
                        continue;
                }

                if (!canopyReplaces(blocks.getBlockAt(x + offsetX, y + offsetY, z + offsetZ)))
                    continue;

                blocks.setBlockStateAt(x + offsetX, y + offsetY, z + offsetZ, leaves);
                placed = true;
            }
        }
    }

    if (tree.mCanopyVineDenominator > 0)
        _hangCanopyVines(blocks, tree, shape, x, y, z, random);
    return placed;
}

void OverworldTreePlacer::_hangCanopyVines(BlockManager &blocks, const Definition &tree, const SimpleCanopy &shape,
                                           int32_t x, int32_t y, int32_t z, std::mt19937 &random) {
    // second pass of sub_1471CB0F0: every leaf of the full squares, corners included, decorated on x - 1, x + 1,
    // z - 1 then z + 1 (sub_1471CB880); a vine goes into air and hangs down through air (sub_1471CCB60)
    static const int32_t SIDES[4][3] = {{-1, 0, 8}, {1, 0, 2}, {0, -1, 1}, {0, 1, 4}};
    static const BlockState vine = VanillaBlocks::VINE().toBlockState();
    const std::string leaves = tree.mLeaves().getIdentifier();
    const auto numerator = static_cast<uint32_t>(tree.mCanopyVineNumerator);
    const auto denominator = static_cast<uint32_t>(tree.mCanopyVineDenominator);
    for (int32_t offsetY = shape.mMinOffset; offsetY <= shape.mMaxOffset; offsetY++) {
        const int32_t radius = shape.mMinWidth
                               + static_cast<int32_t>(static_cast<float>(shape.mMaxOffset - offsetY) * shape.mSlope);
        for (int32_t offsetX = -radius; offsetX <= radius; offsetX++) {
            for (int32_t offsetZ = -radius; offsetZ <= radius; offsetZ++) {
                const int32_t leafX = x + offsetX;
                const int32_t leafY = y + offsetY;
                const int32_t leafZ = z + offsetZ;
                if (blocks.getBlockAt(leafX, leafY, leafZ).mName != leaves)
                    continue;

                for (const int32_t *side: SIDES) {
                    if (numerator != denominator && random() % denominator >= numerator)
                        continue;

                    const int32_t vineX = leafX + side[0];
                    const int32_t vineZ = leafZ + side[1];
                    Tag states = vine.mStates;
                    states.putInt("vine_direction_bits", side[2]);
                    const BlockState hanging(vine.mName, states);
                    int32_t vineY = leafY;
                    for (int32_t step = 0; step < tree.mCanopyVineSteps && isAir(blocks.getBlockAt(vineX, vineY, vineZ));
                         step++)
                        blocks.setBlockStateAt(vineX, vineY--, vineZ, hanging);
                }
            }
        }
    }
}

bool OverworldTreePlacer::_spruceCanopy(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                                        std::mt19937 &random) {
    // SpruceTreeCanopy::place (sub_1471CC3E0): from the ground under the trunk, layers widening by one, back to 0
    // or 1 at each threshold; the threshold grows up to the radius
    int32_t ground = y;
    while (!mayGrowOn(blocks.getBlockAt(x, ground - 1, z))) {
        if (ground <= LevelChunk::MIN_Y)
            return false;
        ground--;
    }

    const int32_t lower = drawRange(random, tree.mLowerOffset);
    const int32_t upper = drawRange(random, tree.mUpperOffset);
    const int32_t maxRadius = drawRange(random, tree.mMaxRadius);
    int32_t radius = nextInt(random, tree.mMaxRadius.mMax - tree.mMaxRadius.mMin);

    const int32_t layers = y + upper - (lower + ground) + 1;
    if (layers == 0)
        return false;

    const BlockState leaves = tree.mLeaves().toBlockState();
    int32_t threshold = tree.mMaxRadius.mMax - 1 - tree.mMaxRadius.mMin;
    int32_t reset = 0;
    bool placed = false;
    for (int32_t layer = 0; layer != layers; layer++) {
        const int32_t leafY = y + upper - layer;
        for (int32_t offsetX = -radius; offsetX <= radius; offsetX++) {
            for (int32_t offsetZ = -radius; offsetZ <= radius; offsetZ++) {
                if (radius != 0 && std::abs(offsetX) == radius && std::abs(offsetZ) == radius)
                    continue;
                if (!canopyReplaces(blocks.getBlockAt(x + offsetX, leafY, z + offsetZ)))
                    continue;

                blocks.setBlockStateAt(x + offsetX, leafY, z + offsetZ, leaves);
                placed = true;
            }
        }

        if (radius >= threshold) {
            radius = reset;
            reset = 1;
            threshold = threshold >= maxRadius ? maxRadius : threshold + 1;
        } else {
            radius++;
        }
    }
    return placed;
}

bool OverworldTreePlacer::_pineCanopy(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                                      std::mt19937 &random) {
    // PineTreeCanopy::place (sub_1471C8820): from one above the trunk down, widening up to the base radius plus a
    // draw, narrowing once near the bottom; its leaves only replace may_replace
    const int32_t height = drawRange(random, tree.mUpperOffset);
    const int32_t limit = tree.mBaseRadius + nextInt(random, height + 1);

    const BlockState leaves = tree.mLeaves().toBlockState();
    bool placed = false;
    int32_t radius = 0;
    for (int32_t offsetY = 1; offsetY >= 1 - height; offsetY--) {
        for (int32_t offsetX = -radius; offsetX <= radius; offsetX++) {
            for (int32_t offsetZ = -radius; offsetZ <= radius; offsetZ++) {
                if (radius != 0 && std::abs(offsetX) == radius && std::abs(offsetZ) == radius)
                    continue;
                if (!mayReplace(blocks.getBlockAt(x + offsetX, y + offsetY, z + offsetZ)))
                    continue;

                blocks.setBlockStateAt(x + offsetX, y + offsetY, z + offsetZ, leaves);
                placed = true;
            }
        }

        if (offsetY == 2 - height && radius > 0)
            radius--;
        else if (radius < limit)
            radius++;
    }
    return placed;
}

void OverworldTreePlacer::_vines(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                                 std::mt19937 &random) {
    // sub_1471CB880 on the four sides, x - 1, x + 1, z - 1 then z + 1; no draw at a chance of 0 or of 100 and more
    if (tree.mVineChance <= 0.0f)
        return;

    static const int32_t SIDES[4][3] = {{-1, 0, 8}, {1, 0, 2}, {0, -1, 1}, {0, 1, 4}};
    for (const int32_t *side: SIDES) {
        if (tree.mVineChance < 100.0f && tree.mVineChance <= nextFloat(random) * 100.0f)
            continue;
        if (!isAir(blocks.getBlockAt(x + side[0], y, z + side[1])))
            continue;

        static const BlockState vine = VanillaBlocks::VINE().toBlockState();
        Tag states = vine.mStates;
        states.putInt("vine_direction_bits", side[2]);
        blocks.setBlockStateAt(x + side[0], y, z + side[1], BlockState(vine.mName, states));
    }
}

bool OverworldTreePlacer::_acaciaTrunk(BlockManager &blocks, const Definition &tree, const Acacia &acacia, int32_t x,
                                       int32_t y, int32_t z, int32_t height, std::mt19937 &random) {
    // AcaciaTreeTrunk::place (sub_14A28D930) for a trunk that does not grow diagonally: the lean only decides which
    // layers are placed, the logs stay above the base, and branches hang under the canopy
    if (acacia.mAllowDiagonalGrowth)
        return _diagonalAcaciaTrunk(blocks, tree, acacia, x, y, z, height, random);
    if (!_canPlace(blocks, tree, x, y, z, height))
        return false;

    const int32_t width = acacia.mTrunkWidth;
    for (int32_t offsetX = 0; offsetX < width; offsetX++) {
        for (int32_t offsetZ = 0; offsetZ < width; offsetZ++) {
            if (!isBaseBlock(blocks.getBlockAt(x + offsetX, y - 1, z + offsetZ)))
                blocks.setBlockStateAt(x + offsetX, y - 1, z + offsetZ, VanillaBlocks::DIRT().toBlockState());
        }
    }

    // the lean direction (the tables at qword_14F0F3D80 and xmmword_14F0F3D90): south, west, north, east
    static const int32_t LEAN_X[4] = {0, -1, 0, 1};
    static const int32_t LEAN_Z[4] = {1, 0, -1, 0};
    const int32_t direction = nextInt(random, 4);
    const int32_t leanHeight = drawRange(random, acacia.mLeanHeight);
    int32_t leanSteps = drawRange(random, acacia.mLeanSteps);
    const int32_t leanLength = drawRange(random, acacia.mLeanLength);

    const BlockState log = tree.mLog().toBlockState();
    int32_t leanX = x;
    int32_t leanY = y;
    int32_t leanZ = z;
    bool topped = false;
    int32_t topX = 0;
    int32_t topY = 0;
    int32_t topZ = 0;
    for (int32_t step = 0; step < height + leanLength; step++) {
        if (step >= height - leanHeight && leanSteps > 0) {
            leanX += LEAN_X[direction];
            leanZ += LEAN_Z[direction];
            leanSteps--;
        }
        if (step < height)
            leanY = y + step;
        if (!logReplaces(tree, blocks.getBlockAt(leanX, leanY, leanZ)))
            continue;

        for (int32_t offsetX = 0; offsetX < width; offsetX++) {
            for (int32_t offsetZ = 0; offsetZ < width; offsetZ++) {
                const int32_t logX = x + offsetX;
                const int32_t logY = y + step;
                const int32_t logZ = z + offsetZ;
                if (!logReplaces(tree, blocks.getBlockAt(logX, logY, logZ)))
                    continue;

                blocks.setBlockStateAt(logX, logY, logZ, log);
                const bool sides[4] = {offsetX == 0, offsetX != 0 && offsetX == width - 1, offsetZ == 0,
                                       offsetZ != 0 && offsetZ == width - 1};
                _decorateLog(blocks, acacia.mVineNumerator, acacia.mVineDenominator, logX, logY, logZ, sides, random);
                if (step < acacia.mMinHeightForCanopy)
                    continue;

                topped = true;
                topX = logX;
                topY = logY;
                topZ = logZ;
            }
        }
    }

    // the canopy sits on the last log placed, the corner of the top layer farthest from the base
    if (topped && tree.mCanopy == Canopy::Roofed)
        _roofedCanopy(blocks, tree, acacia, topX, topY, topZ, random);

    for (int32_t offsetX = -1; offsetX <= width; offsetX++) {
        for (int32_t offsetZ = -1; offsetZ <= width; offsetZ++) {
            if (offsetX >= 0 && offsetZ >= 0 && offsetX < width && offsetZ < width)
                continue;
            if (acacia.mBranchDenominator == 0)
                continue;
            if (acacia.mBranchNumerator != acacia.mBranchDenominator
                && random() % acacia.mBranchDenominator >= acacia.mBranchNumerator)
                continue;

            const int32_t length = drawRange(random, acacia.mBranchLength);
            const int32_t position = drawRange(random, acacia.mBranchPosition);
            // the logs hang beside the base column, their canopy is offset from the top log instead
            for (int32_t step = 0; step < length; step++) {
                const int32_t logY = topY - step - position;
                if (logReplaces(tree, blocks.getBlockAt(x + offsetX, logY, z + offsetZ)))
                    blocks.setBlockStateAt(x + offsetX, logY, z + offsetZ, log);
            }
            _acaciaCanopy(blocks, tree, acacia.mBranchCanopySize, acacia.mBranchCanopySimplified, topX + offsetX,
                          topY - position, topZ + offsetZ);
        }
    }
    return true;
}

bool OverworldTreePlacer::_mangroveRoots(BlockManager &blocks, const Definition &tree, const Mangrove &mangrove,
                                         int32_t x, int32_t y, int32_t z, int32_t height, std::mt19937 &random,
                                         int32_t &trunkY) {
    // MangroveTreeRoots::place (sub_1471C4B90): the trunk rises y_offset above the position over a clear column,
    // a root under it and one path from each side; a path too long fails the tree
    trunkY = y + drawRange(random, mangrove.mRootOffset);
    if (!_canPlace(blocks, tree, x, y, z, trunkY - y + height))
        return false;
    for (int32_t at = y; at < trunkY; at++) {
        if (!rootPasses(blocks.getBlockAt(x, at, z)))
            return false;
    }

    // north, east, south and west (qword_14F0ACEB8), their facings 2, 5, 3 and 4
    static const int32_t SIDES[4][3] = {{0, -1, 2}, {1, 0, 5}, {0, 1, 3}, {-1, 0, 4}};
    const std::array<int32_t, 3> trunk = {x, trunkY, z};
    std::vector<std::array<int32_t, 3>> roots = {{x, trunkY - 1, z}};
    for (const int32_t *side: SIDES) {
        const std::array<int32_t, 3> start = {x + side[0], trunkY, z + side[1]};
        std::vector<std::array<int32_t, 3>> path;
        _rootPath(blocks, mangrove, start, trunk, side[2], path, 0, random);
        if (path.empty() || static_cast<int32_t>(path.size()) > mangrove.mMaxRootLength)
            return false;
        roots.insert(roots.end(), path.begin(), path.end());
        roots.push_back(start);
    }

    const BlockState mud = VanillaBlocks::MUD().toBlockState();
    const BlockState root = VanillaBlocks::MANGROVE_ROOTS().toBlockState();
    const BlockState muddyRoot = VanillaBlocks::MUDDY_MANGROVE_ROOTS().toBlockState();
    const BlockState moss = VanillaBlocks::MOSS_CARPET().toBlockState();
    for (const std::array<int32_t, 3> &at: roots) {
        const BlockState &state = blocks.getBlockAt(at[0], at[1], at[2]);
        if (!rootPasses(state))
            continue;
        if (state.mName == mud.mName) {
            blocks.setBlockStateAt(at[0], at[1], at[2], muddyRoot);
            continue;
        }

        blocks.setBlockStateAt(at[0], at[1], at[2], root);
        const uint32_t numerator = mangrove.mMossNumerator;
        const uint32_t denominator = mangrove.mMossDenominator;
        if (denominator == 0 || (numerator != denominator && random() % denominator >= numerator))
            continue;
        if (isAir(blocks.getBlockAt(at[0], at[1] + 1, at[2])))
            blocks.setBlockStateAt(at[0], at[1] + 1, at[2], moss);
    }
    return true;
}

bool OverworldTreePlacer::_rootPath(BlockManager &blocks, const Mangrove &mangrove, const std::array<int32_t, 3> &from,
                                    const std::array<int32_t, 3> &trunk, int32_t direction,
                                    std::vector<std::array<int32_t, 3>> &path, int32_t depth, std::mt19937 &random) {
    // sub_1471C5910: down, or one step out from near the trunk, away from it a step out and down at times; the
    // length limit clears the whole path
    if (depth == mangrove.mMaxRootLength || static_cast<int32_t>(path.size()) > mangrove.mMaxRootLength) {
        path.clear();
        return false;
    }

    const int32_t stepX = direction == 5 ? 1 : (direction == 4 ? -1 : 0);
    const int32_t stepZ = direction == 3 ? 1 : (direction == 2 ? -1 : 0);
    const std::array<int32_t, 3> below = {from[0], from[1] - 1, from[2]};
    const std::array<int32_t, 3> out = {from[0] + stepX, from[1], from[2] + stepZ};
    const int32_t distance = std::abs(trunk[0] - from[0]) + std::abs(trunk[1] - from[1]) + std::abs(trunk[2] - from[2]);
    const int32_t width = mangrove.mMaxRootWidth;
    std::vector<std::array<int32_t, 3>> candidates;
    if (distance <= width - 3 || distance > width) {
        if (distance <= width && nextFloat(random) >= 0.2f && nextBoolean(random))
            candidates.push_back(out);
        else
            candidates.push_back(below);
    } else {
        candidates.push_back(below);
        if (nextFloat(random) < 0.2f)
            candidates.push_back({out[0], out[1] - 1, out[2]});
    }

    for (const std::array<int32_t, 3> &next: candidates) {
        if (!rootPasses(blocks.getBlockAt(next[0], next[1], next[2])))
            continue;
        path.push_back(next);
        if (!_rootPath(blocks, mangrove, next, trunk, direction, path, depth + 1, random))
            return false;
    }
    return true;
}

void OverworldTreePlacer::_mangroveTrunk(BlockManager &blocks, const Definition &tree, const Mangrove &mangrove,
                                         int32_t x, int32_t y, int32_t z, int32_t height, std::mt19937 &random) {
    // the mangrove trunk (sub_1471C5E70): each log but the last may start a branch one time in two, its logs and
    // their ends kept for the canopy with the top of the trunk
    const auto inclusive = [&](const Range &range) {
        return range.mMax > range.mMin ? range.mMin + nextInt(random, range.mMax - range.mMin + 1) : range.mMin;
    };
    const BlockState log = tree.mLog().toBlockState();
    std::vector<std::array<int32_t, 3>> attachments;
    for (int32_t step = 0; step < height; step++) {
        const int32_t logY = y + step;
        if (logReplaces(tree, blocks.getBlockAt(x, logY, z))) {
            blocks.setBlockStateAt(x, logY, z, log);
            if (step < height - 1 && nextBoolean(random)) {
                const int32_t direction = nextInt(random, 4);
                const int32_t first = inclusive(mangrove.mBranchLength);
                const int32_t second = inclusive(mangrove.mBranchLength);
                int32_t steps = inclusive(mangrove.mBranchSteps);
                int32_t offset = std::max(first - second - 1, 0);
                int32_t top = offset + logY;
                // directions 0 north, 1 south, 2 west, 3 east
                const int32_t stepX = direction == 3 ? 1 : (direction == 2 ? -1 : 0);
                const int32_t stepZ = direction == 0 ? -1 : (direction == 1 ? 1 : 0);
                int32_t branchX = x;
                int32_t branchZ = z;
                if (offset < height && steps > 0) {
                    while (true) {
                        if (offset != 0) {
                            branchX += stepX;
                            branchZ += stepZ;
                            const int32_t branchY = offset + logY;
                            top = branchY;
                            if (logReplaces(tree, blocks.getBlockAt(branchX, branchY, branchZ))) {
                                blocks.setBlockStateAt(branchX, branchY, branchZ, log);
                                top = branchY + 1;
                            }
                            attachments.push_back({branchX, branchY, branchZ});
                        }
                        if (++offset >= height || steps-- <= 1)
                            break;
                    }
                }
                if (top - logY > 1) {
                    attachments.push_back({branchX, top, branchZ});
                    attachments.push_back({branchX, top - 2, branchZ});
                }
            }
        }
        if (step == height - 1)
            attachments.push_back({x, logY + 1, z});
    }

    if (tree.mCanopy == Canopy::Mangrove)
        _mangroveCanopy(blocks, tree, mangrove, attachments, random);
}

void OverworldTreePlacer::_mangroveCanopy(BlockManager &blocks, const Definition &tree, const Mangrove &mangrove,
                                          const std::vector<std::array<int32_t, 3>> &attachments,
                                          std::mt19937 &random) {
    // MangroveTreeCanopy::place (vtable 0x14F3D6940, sub_14A294000): leaf_placement_attempts spread draws around
    // every attachment, the leaves shuffled, vines hung from them, then a hanging propagule under some, none within
    // 1 of another and each over two blocks of air (defaults of sub_14A293D50)
    const auto inclusive = [&](const Range &range) {
        return range.mMin <= range.mMax ? range.mMin + nextInt(random, range.mMax - range.mMin + 1) : range.mMin;
    };
    const int32_t height = inclusive(mangrove.mCanopyHeight);
    const int32_t radius = inclusive(mangrove.mCanopyRadius);
    const BlockState leaves = tree.mLeaves().toBlockState();
    std::vector<std::array<int32_t, 3>> placed;
    for (const std::array<int32_t, 3> &at: attachments) {
        for (int32_t attempt = 0; attempt < mangrove.mLeafAttempts; attempt++) {
            // the weighted leaf_blocks draw, one block of weight 1
            nextInt(random, 1);
            const int32_t awayX = nextInt(random, radius);
            const int32_t leafX = at[0] + awayX - nextInt(random, radius);
            const int32_t awayY = nextInt(random, height);
            const int32_t leafY = at[1] + awayY - nextInt(random, height);
            const int32_t awayZ = nextInt(random, radius);
            const int32_t leafZ = at[2] + awayZ - nextInt(random, radius);
            if (!logReplaces(tree, blocks.getBlockAt(leafX, leafY, leafZ)))
                continue;
            blocks.setBlockStateAt(leafX, leafY, leafZ, leaves);
            placed.push_back({leafX, leafY, leafZ});
        }
    }
    for (size_t index = 1; index < placed.size(); index++) {
        const size_t other = random() % static_cast<uint32_t>(index + 1);
        if (other != index)
            std::swap(placed[index], placed[other]);
    }

    if (mangrove.mVineDenominator > 0) {
        static const int32_t SIDES[4][3] = {{-1, 0, 8}, {1, 0, 2}, {0, -1, 1}, {0, 1, 4}};
        for (const std::array<int32_t, 3> &leaf: placed) {
            for (const int32_t *side: SIDES) {
                if (mangrove.mVineNumerator != mangrove.mVineDenominator
                    && random() % mangrove.mVineDenominator >= mangrove.mVineNumerator)
                    continue;
                _hangVine(blocks, leaf[0] + side[0], leaf[1], leaf[2] + side[1], side[2], mangrove.mVineSteps);
            }
        }
    }

    if (mangrove.mPropaguleDenominator == 0)
        return;
    BlockState propagule = VanillaBlocks::MANGROVE_PROPAGULE().toBlockState();
    Tag states = propagule.mStates;
    states.putByte("hanging", 1);
    propagule = BlockState(propagule.mName, states);
    std::vector<std::array<int32_t, 3>> excluded;
    const auto isExcluded = [&](const std::array<int32_t, 3> &at) {
        return std::find(excluded.begin(), excluded.end(), at) != excluded.end();
    };
    for (const std::array<int32_t, 3> &leaf: placed) {
        const std::array<int32_t, 3> below = {leaf[0], leaf[1] - 1, leaf[2]};
        if (isExcluded(below))
            continue;
        if (mangrove.mPropaguleNumerator != mangrove.mPropaguleDenominator
            && random() % mangrove.mPropaguleDenominator >= mangrove.mPropaguleNumerator)
            continue;
        if (!isAir(blocks.getBlockAt(leaf[0], leaf[1] - 1, leaf[2]))
            || !isAir(blocks.getBlockAt(leaf[0], leaf[1] - 2, leaf[2])))
            continue;

        for (int32_t offsetX = -1; offsetX <= 1; offsetX++) {
            for (int32_t offsetY = -1; offsetY <= 1; offsetY++) {
                for (int32_t offsetZ = -1; offsetZ <= 1; offsetZ++)
                    excluded.push_back({below[0] + offsetX, below[1] + offsetY, below[2] + offsetZ});
            }
        }
        blocks.setBlockStateAt(below[0], below[1], below[2], propagule);
    }
}

void OverworldTreePlacer::_hangVine(BlockManager &blocks, int32_t x, int32_t y, int32_t z, int32_t bits,
                                    int32_t steps) {
    // sub_1471CCB60: a vine into air, then down through air for the steps left
    static const BlockState vine = VanillaBlocks::VINE().toBlockState();
    Tag states = vine.mStates;
    states.putInt("vine_direction_bits", bits);
    const BlockState hanging(vine.mName, states);
    for (int32_t step = 0; step < steps && isAir(blocks.getBlockAt(x, y - step, z)); step++)
        blocks.setBlockStateAt(x, y - step, z, hanging);
}

bool OverworldTreePlacer::_diagonalAcaciaTrunk(BlockManager &blocks, const Definition &tree, const Acacia &acacia,
                                               int32_t x, int32_t y, int32_t z, int32_t height, std::mt19937 &random) {
    // sub_14A28D930 with allow_diagonal_growth, for a trunk of width 1: the logs follow the lean, the canopy sits on
    // the last log, then one branch from the base column the other way, unless it draws the lean's direction
    if (!_canPlace(blocks, tree, x, y, z, height) || y >= LevelChunk::MAX_Y + 1 - height - 1)
        return false;
    if (!isBaseBlock(blocks.getBlockAt(x, y - 1, z)))
        blocks.setBlockStateAt(x, y - 1, z, VanillaBlocks::DIRT().toBlockState());

    // south, west, north, east (qword_14F0F3D80, xmmword_14F0F3D90)
    static const int32_t LEAN_X[4] = {0, -1, 0, 1};
    static const int32_t LEAN_Z[4] = {1, 0, -1, 0};
    const int32_t direction = nextInt(random, 4);
    const int32_t leanStart = height - drawRange(random, acacia.mLeanHeight);
    int32_t leanSteps = drawRange(random, acacia.mLeanSteps);
    const int32_t leanLength = drawRange(random, acacia.mLeanLength);

    const BlockState log = tree.mLog().toBlockState();
    static const bool SIDES[4] = {true, false, true, false};
    int32_t leanX = x;
    int32_t leanY = y;
    int32_t leanZ = z;
    bool topped = false;
    int32_t topX = x;
    int32_t topY = y;
    int32_t topZ = z;
    for (int32_t step = 0; step < height + leanLength; step++) {
        if (step >= leanStart && leanSteps > 0) {
            leanX += LEAN_X[direction];
            leanZ += LEAN_Z[direction];
            leanSteps--;
        }
        if (step < height)
            leanY = y + step;
        if (!logReplaces(tree, blocks.getBlockAt(leanX, leanY, leanZ)))
            continue;

        blocks.setBlockStateAt(leanX, leanY, leanZ, log);
        // the width-1 trunk only decorates its -x and -z sides
        _decorateLog(blocks, acacia.mVineNumerator, acacia.mVineDenominator, leanX, leanY, leanZ, SIDES, random);
        if (step < acacia.mMinHeightForCanopy)
            continue;
        topped = true;
        topX = leanX;
        topY = leanY;
        topZ = leanZ;
    }
    if (topped && tree.mCanopy == Canopy::Acacia)
        _acaciaCanopy(blocks, tree, acacia.mCanopySize, false, topX, topY, topZ);

    const int32_t branchDirection = nextInt(random, 4);
    if (branchDirection == direction)
        return true;
    if (acacia.mBranchDenominator == 0
        || (acacia.mBranchNumerator != acacia.mBranchDenominator
            && random() % acacia.mBranchDenominator >= acacia.mBranchNumerator))
        return true;

    const int32_t position = drawRange(random, acacia.mBranchPosition);
    int32_t length = drawRange(random, acacia.mBranchLength);
    int32_t layer = leanStart - position;
    if (layer >= height || length <= 0)
        return true;

    // each step moves the branch out one block, then up one layer once a log went in
    int32_t branchX = x;
    int32_t branchZ = z;
    int32_t branchY = topY;
    bool grown = false;
    while (true) {
        bool ended = false;
        while (layer > 0) {
            branchX += LEAN_X[branchDirection];
            branchZ += LEAN_Z[branchDirection];
            const int32_t atY = y + layer;
            if (!logReplaces(tree, blocks.getBlockAt(branchX, atY, branchZ)))
                break;

            static const bool ALL_SIDES[4] = {true, true, true, true};
            blocks.setBlockStateAt(branchX, atY, branchZ, log);
            _decorateLog(blocks, acacia.mVineNumerator, acacia.mVineDenominator, branchX, atY, branchZ, ALL_SIDES,
                         random);
            branchY = atY;
            if (layer + 1 < height) {
                layer++;
                grown = true;
                if (length-- > 1)
                    continue;
            }
            ended = true;
            break;
        }
        if (ended) {
            grown = true;
            break;
        }
        if (++layer >= height || length-- <= 1)
            break;
    }
    if (grown)
        _acaciaCanopy(blocks, tree, acacia.mBranchCanopySize, acacia.mBranchCanopySimplified, branchX, branchY,
                      branchZ);
    return true;
}

void OverworldTreePlacer::_decorateLog(BlockManager &blocks, uint32_t numerator, uint32_t denominator, int32_t x,
                                       int32_t y, int32_t z, const bool sides[4], std::mt19937 &random) {
    // sub_1471CB880: each outer side draws its chance, then a vine goes into air (sub_1471CCB60)
    if (denominator == 0)
        return;

    static const int32_t SIDES[4][3] = {{-1, 0, 8}, {1, 0, 2}, {0, -1, 1}, {0, 1, 4}};
    static const BlockState vine = VanillaBlocks::VINE().toBlockState();
    for (size_t side = 0; side < 4; side++) {
        if (!sides[side])
            continue;
        if (numerator != denominator && random() % denominator >= numerator)
            continue;

        const int32_t vineX = x + SIDES[side][0];
        const int32_t vineZ = z + SIDES[side][1];
        if (!isAir(blocks.getBlockAt(vineX, y, vineZ)))
            continue;

        Tag states = vine.mStates;
        states.putInt("vine_direction_bits", SIDES[side][2]);
        blocks.setBlockStateAt(vineX, y, vineZ, BlockState(vine.mName, states));
    }
}

void OverworldTreePlacer::_roofedCanopy(BlockManager &blocks, const Definition &tree, const Acacia &acacia, int32_t x,
                                        int32_t y, int32_t z, std::mt19937 &random) {
    // RoofedTreeCanopy::place (sub_1471CA840): two rings around the core below and above the layers, the core
    // capped one time in two, then the layers; its leaves only go into air (material 0)
    const BlockState leaves = tree.mLeaves().toBlockState();
    const auto leaf = [&](int32_t leafX, int32_t leafY, int32_t leafZ) {
        if (isAir(blocks.getBlockAt(leafX, leafY, leafZ)))
            blocks.setBlockStateAt(leafX, leafY, leafZ, leaves);
    };

    // the parser keeps canopy_height minus 3 (sub_1472C08C0)
    const int32_t layers = acacia.mCanopyHeight - 3;
    const int32_t core = acacia.mCanopyCoreWidth;
    const int32_t outer = acacia.mCanopyOuterRadius;
    const int32_t inner = acacia.mCanopyInnerRadius;
    for (int32_t offsetX = -outer; offsetX <= 0; offsetX++) {
        for (int32_t offsetZ = -outer; offsetZ <= 0; offsetZ++) {
            const int32_t farX = core - 1 - offsetX;
            const int32_t farZ = core - 1 - offsetZ;
            leaf(x + offsetX, y - 1, z + offsetZ);
            leaf(x + farX, y - 1, z + offsetZ);
            leaf(x + offsetX, y - 1, z + farZ);
            leaf(x + farX, y - 1, z + farZ);
            if ((offsetZ > 1 - outer || offsetX > -outer) && (offsetX != 1 - outer || offsetZ != -outer)) {
                leaf(x + offsetX, y + layers, z + offsetZ);
                leaf(x + farX, y + layers, z + offsetZ);
                leaf(x + offsetX, y + layers, z + farZ);
                leaf(x + farX, y + layers, z + farZ);
            }
        }
    }

    if (nextBoolean(random)) {
        for (int32_t offsetX = 0; offsetX < core; offsetX++) {
            for (int32_t offsetZ = 0; offsetZ < core; offsetZ++)
                leaf(x + offsetX, y + layers + 1, z + offsetZ);
        }
    }

    const int32_t farX = inner + core - 1;
    const int32_t farZ = inner + core - 1;
    for (int32_t layer = 0; layer < layers; layer++) {
        for (int32_t offsetX = -inner; offsetX <= farX; offsetX++) {
            for (int32_t offsetZ = -inner; offsetZ <= farZ; offsetZ++) {
                if (offsetX == -inner && (offsetZ == -inner || offsetZ == farZ))
                    continue;
                if (offsetX == farX && (offsetZ == farZ || offsetZ == -inner))
                    continue;
                if (std::abs(offsetX) < inner || std::abs(offsetZ) < inner)
                    leaf(x + offsetX, y + layer, z + offsetZ);
            }
        }
    }
}

void OverworldTreePlacer::_acaciaCanopy(BlockManager &blocks, const Definition &tree, int32_t size, bool simplified,
                                        int32_t x, int32_t y, int32_t z) {
    // AcaciaTreeCanopy::place (sub_14A28D2E0): a top layer one above, then a disc without its corners, the leaves
    // replacing may_replace only
    const BlockState leaves = tree.mLeaves().toBlockState();
    const auto leaf = [&](int32_t leafX, int32_t leafY, int32_t leafZ) {
        if (replaces(tree, blocks.getBlockAt(leafX, leafY, leafZ)))
            blocks.setBlockStateAt(leafX, leafY, leafZ, leaves);
    };

    if (simplified) {
        const int32_t top = size >= 2 ? size : 1;
        for (int32_t offsetX = 1 - top; offsetX < top; offsetX++) {
            for (int32_t offsetZ = 1 - top; offsetZ < top; offsetZ++)
                leaf(x + offsetX, y + 1, z + offsetZ);
        }
    } else {
        // a square of size - 1 above, and the four blocks one past it along the axes
        const int32_t reach = std::max(size, 2) - 1;
        for (int32_t offsetX = 1 - reach; offsetX < reach; offsetX++) {
            for (int32_t offsetZ = 1 - reach; offsetZ < reach; offsetZ++)
                leaf(x + offsetX, y + 1, z + offsetZ);
        }
        const int32_t past = std::max(size, 1) - 1;
        leaf(x + past, y + 1, z);
        leaf(x - past, y + 1, z);
        leaf(x, y + 1, z + past);
        leaf(x, y + 1, z - past);
    }
    for (int32_t offsetX = -size; offsetX <= size; offsetX++) {
        for (int32_t offsetZ = -size; offsetZ <= size; offsetZ++) {
            if (std::abs(offsetX) != size || std::abs(offsetZ) != size)
                leaf(x + offsetX, y, z + offsetZ);
        }
    }
}
