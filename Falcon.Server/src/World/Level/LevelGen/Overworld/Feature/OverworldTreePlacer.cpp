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

    bool replaces(const OverworldTreePlacer::Definition &tree, const BlockState &state) {
        if (tree.mAirAndLeaves)
            return isAir(state) || isLeaves(state);
        return mayReplace(state) || (tree.mInWater && isWater(state));
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
                                const Decorate &decorate) {
    // VanillaTreeFeature::place (sub_14A2C9A20): the trunk height first, then the trunk, which places the canopy
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

    const int32_t height = drawRange(random, tree.mHeight);
    if (tree.mTrunk == Trunk::Fallen)
        return _fallenTrunk(blocks, tree, x, y, z, height, random, decorate);
    return _simpleTrunk(blocks, tree, x, y, z, height, random);
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
                if (!replaces(tree, state) && (tree.mInWater || !mayGrowOn(state)))
                    return false;
            }
        }
    }
    return true;
}

bool OverworldTreePlacer::_simpleTrunk(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                                       int32_t height, std::mt19937 &random) {
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
    // MegaTreeTrunk::place (sub_1471C7140) for a trunk without branches or decoration
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

    // the canopy goes first, from the top of the trunk
    bool placed = _megaPineCanopy(blocks, tree, mega, x, y + height, z, random);

    const BlockState log = tree.mLog().toBlockState();
    const auto placeLog = [&](int32_t logX, int32_t logY, int32_t logZ) {
        if (!mayReplace(blocks.getBlockAt(logX, logY, logZ)))
            return;
        blocks.setBlockStateAt(logX, logY, logZ, log);
        placed = true;
    };
    // the ground layer, the layers up to two below the top, then one log at the top corner
    for (int32_t offsetX = 0; offsetX < width; offsetX++) {
        for (int32_t at = y; at <= y + height - 2; at++) {
            for (int32_t offsetZ = 0; offsetZ < width; offsetZ++)
                placeLog(x + offsetX, at, z + offsetZ);
        }
    }
    placeLog(x, y + height - 1, z);

    const BlockState base = mega.mBaseBlock().toBlockState();
    for (int32_t offsetX = 0; offsetX < width; offsetX++) {
        for (int32_t offsetZ = 0; offsetZ < width; offsetZ++) {
            if (blocks.getBlockAt(x + offsetX, y - 1, z + offsetZ).mName != base.mName)
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
    // SimpleTreeCanopy::place (sub_1471CB0F0) with the vanilla canopy: offsets -3 to 0 and a slope of 1 / 2, corners
    // skipped one time in two below the top layer and always on it
    const int32_t minOffset = -3;
    const int32_t maxOffset = 0;
    const int32_t minWidth = tree.mCanopyMinWidth;
    const float slope = 0.5f;
    static const std::array<std::pair<uint32_t, uint32_t>, 4> VARIATION = {{{1, 2}, {1, 2}, {1, 2}, {1, 1}}};

    const BlockState leaves = tree.mLeaves().toBlockState();
    const int32_t base = minWidth + static_cast<int32_t>(static_cast<float>(maxOffset) * slope);
    bool placed = false;
    for (int32_t offsetY = minOffset; offsetY <= maxOffset; offsetY++) {
        const int32_t radius = base - static_cast<int32_t>(static_cast<float>(offsetY) * slope);
        for (int32_t offsetX = -radius; offsetX <= radius; offsetX++) {
            for (int32_t offsetZ = -radius; offsetZ <= radius; offsetZ++) {
                if (std::abs(offsetX) == radius && std::abs(offsetZ) == radius) {
                    const std::pair<uint32_t, uint32_t> &chance = VARIATION[(size_t) (offsetY - minOffset)];
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
        _hangCanopyVines(blocks, tree, x, y, z, minOffset, maxOffset, slope, random);
    return placed;
}

void OverworldTreePlacer::_hangCanopyVines(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y,
                                           int32_t z, int32_t minOffset, int32_t maxOffset, float slope,
                                           std::mt19937 &random) {
    // second pass of sub_1471CB0F0: every leaf of the full squares, corners included, decorated on x - 1, x + 1,
    // z - 1 then z + 1 (sub_1471CB880); a vine goes into air and hangs down through air (sub_1471CCB60)
    static const int32_t SIDES[4][3] = {{-1, 0, 8}, {1, 0, 2}, {0, -1, 1}, {0, 1, 4}};
    static const BlockState vine = VanillaBlocks::VINE().toBlockState();
    const std::string leaves = tree.mLeaves().getIdentifier();
    const auto numerator = static_cast<uint32_t>(tree.mCanopyVineNumerator);
    const auto denominator = static_cast<uint32_t>(tree.mCanopyVineDenominator);
    for (int32_t offsetY = minOffset; offsetY <= maxOffset; offsetY++) {
        const int32_t radius = tree.mCanopyMinWidth
                               + static_cast<int32_t>(static_cast<float>(maxOffset - offsetY) * slope);
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
    if (acacia.mAllowDiagonalGrowth || !_canPlace(blocks, tree, x, y, z, height))
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
                _decorateLog(blocks, acacia, logX, logY, logZ, sides, random);
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

void OverworldTreePlacer::_decorateLog(BlockManager &blocks, const Acacia &acacia, int32_t x, int32_t y, int32_t z,
                                       const bool sides[4], std::mt19937 &random) {
    // sub_1471CB880: each outer side draws its chance, then a vine goes into air (sub_1471CCB60)
    if (acacia.mVineDenominator == 0)
        return;

    static const int32_t SIDES[4][3] = {{-1, 0, 8}, {1, 0, 2}, {0, -1, 1}, {0, 1, 4}};
    static const BlockState vine = VanillaBlocks::VINE().toBlockState();
    for (size_t side = 0; side < 4; side++) {
        if (!sides[side])
            continue;
        if (acacia.mVineNumerator != acacia.mVineDenominator && random() % acacia.mVineDenominator >= acacia.mVineNumerator)
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
    // AcaciaTreeCanopy::place (sub_14A28D2E0) with simplify_canopy: a square one above, then a disc without its
    // corners, the leaves replacing may_replace only
    const BlockState leaves = tree.mLeaves().toBlockState();
    const auto leaf = [&](int32_t leafX, int32_t leafY, int32_t leafZ) {
        if (replaces(tree, blocks.getBlockAt(leafX, leafY, leafZ)))
            blocks.setBlockStateAt(leafX, leafY, leafZ, leaves);
    };

    if (!simplified)
        return;

    const int32_t top = size >= 2 ? size : 1;
    for (int32_t offsetX = 1 - top; offsetX < top; offsetX++) {
        for (int32_t offsetZ = 1 - top; offsetZ < top; offsetZ++)
            leaf(x + offsetX, y + 1, z + offsetZ);
    }
    for (int32_t offsetX = -size; offsetX <= size; offsetX++) {
        for (int32_t offsetZ = -size; offsetZ <= size; offsetZ++) {
            if (std::abs(offsetX) != size || std::abs(offsetZ) != size)
                leaf(x + offsetX, y, z + offsetZ);
        }
    }
}
