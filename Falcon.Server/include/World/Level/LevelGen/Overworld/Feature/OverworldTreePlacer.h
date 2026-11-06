#pragma once

#include "World/Level/Block/Block.h"

#include <cstdint>
#include <functional>
#include <random>
#include <vector>

class BlockManager;

// the game's minecraft:tree_feature with the trunks and canopies of the overworld's vanilla trees
// (VanillaTreeFeature, place sub_14A2C9A20); every draw comes from the placement twister it is given
class OverworldTreePlacer {
public:
    enum class Trunk {
        Simple,
        Fancy,
        Fallen,
        Mega
    };

    enum class Canopy {
        None,
        Simple,
        Fancy,
        Spruce,
        Pine,
        MegaPine
    };

    // a range of the feature's JSON, its maximum excluded
    struct Range {
        int32_t mMin;
        int32_t mMax;
    };

    struct Definition {
        const char *mIdentifier;
        Trunk mTrunk;
        Block (*mLog)();
        // the trunk height, or the log length of a fallen trunk
        Range mHeight;
        Range mHeightModifier;
        Canopy mCanopy;
        Block (*mLeaves)();
        // spruce canopy offsets and radius, or the pine canopy height
        Range mLowerOffset;
        Range mUpperOffset;
        Range mMaxRadius;
        int32_t mBaseRadius;
        // the vine every side of each log gets, as a percentage
        float mVineChance;
        // a fallen trunk places its log decoration feature on each log
        bool mLogDecoration;
        // can_be_submerged max_depth: the trunk sinks through up to this many replaceable blocks
        int32_t mMaxDepth = 0;
        int32_t mCanopyMinWidth = 1;
        // canopy_decoration: each side of every leaf hangs a vine, numerator in denominator, num_steps down
        int32_t mCanopyVineNumerator = 0;
        int32_t mCanopyVineDenominator = 0;
        int32_t mCanopyVineSteps = 0;
        // water among may_replace, and no may_grow_through
        bool mInWater = false;
    };

    // fancy_trunk and fancy_canopy of fancy_oak_tree_feature
    struct Fancy {
        int32_t mBase;
        int32_t mVariance;
        float mScale;
        int32_t mTrunkWidth;
        float mBranchSlope;
        float mBranchDensity;
        float mMinAltitudeFactor;
        float mWidthScale;
        float mFoliageAltitudeFactor;
        int32_t mCanopyHeight;
        int32_t mCanopyRadius;
    };

    // mega_trunk without branches, mega_pine_canopy and the base_cluster of the mega spruce and pine features
    struct Mega {
        int32_t mTrunkWidth;
        int32_t mBaseHeight;
        std::vector<int32_t> mHeightIntervals;
        Range mCanopyHeight;
        int32_t mBaseRadius;
        float mRadiusStepModifier;
        int32_t mCoreWidth;
        Block (*mBaseBlock)();
        int32_t mClusters;
        int32_t mClusterRadius;
    };

    using Decorate = std::function<void(int32_t, int32_t, int32_t)>;

    static bool place(BlockManager &blocks, const Definition &tree, const Fancy *fancy, const Mega *mega, int32_t x,
                      int32_t y, int32_t z, std::mt19937 &random, const Decorate &decorate);

private:
    static bool _canPlace(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                          int32_t height);

    static bool _simpleTrunk(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                             int32_t height, std::mt19937 &random);

    static bool _fancyTrunk(BlockManager &blocks, const Definition &tree, const Fancy &fancy, int32_t x, int32_t y,
                            int32_t z, int32_t height, std::mt19937 &random);

    static bool _fallenTrunk(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                             int32_t length, std::mt19937 &random, const Decorate &decorate);

    static bool _megaTrunk(BlockManager &blocks, const Definition &tree, const Mega &mega, int32_t x, int32_t y,
                           int32_t z, int32_t height, std::mt19937 &random);

    static void _baseCluster(BlockManager &blocks, const Mega &mega, int32_t x, int32_t y, int32_t z);

    static bool _megaPineCanopy(BlockManager &blocks, const Definition &tree, const Mega &mega, int32_t x, int32_t y,
                                int32_t z, std::mt19937 &random);

    static bool _canopy(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                        std::mt19937 &random);

    static bool _simpleCanopy(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                              std::mt19937 &random);

    static bool _spruceCanopy(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                              std::mt19937 &random);

    static bool _pineCanopy(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                            std::mt19937 &random);

    static void _hangCanopyVines(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                                 int32_t minOffset, int32_t maxOffset, float slope, std::mt19937 &random);

    static void _fancyLayer(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                            int32_t radius);

    static void _vines(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                       std::mt19937 &random);
};
