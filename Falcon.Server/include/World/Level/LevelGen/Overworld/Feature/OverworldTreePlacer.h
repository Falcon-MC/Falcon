#pragma once

#include "World/Level/Block/Block.h"

#include <array>
#include <cstdint>
#include <functional>
#include <random>
#include <utility>
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
        Mega,
        Acacia,
        Mangrove,
        Cherry
    };

    enum class Canopy {
        None,
        Simple,
        Fancy,
        Spruce,
        Pine,
        MegaPine,
        Mega,
        Roofed,
        Acacia,
        Mangrove,
        Cherry
    };

    // a range of the feature's JSON, its maximum excluded
    struct Range {
        int32_t mMin;
        int32_t mMax;
    };

    // canopy_offset, min_width, canopy_slope and variation_chance of a simple canopy, one variation per layer
    struct SimpleCanopy {
        int32_t mMinOffset;
        int32_t mMaxOffset;
        int32_t mMinWidth;
        float mSlope;
        std::vector<std::pair<uint32_t, uint32_t>> mVariation;
    };

    // mangrove_roots, mangrove_trunk and mangrove_canopy; the inclusive ranges keep their maximum
    struct Mangrove {
        int32_t mMaxRootWidth;
        int32_t mMaxRootLength;
        // y_offset, its maximum excluded
        Range mRootOffset;
        // above_root moss carpet chance
        uint32_t mMossNumerator;
        uint32_t mMossDenominator;
        int32_t mBaseHeight;
        int32_t mHeightRandA;
        int32_t mHeightRandB;
        Range mBranchLength;
        Range mBranchSteps;
        Range mCanopyHeight;
        Range mCanopyRadius;
        int32_t mLeafAttempts;
        uint32_t mVineNumerator;
        uint32_t mVineDenominator;
        int32_t mVineSteps;
        uint32_t mPropaguleNumerator;
        uint32_t mPropaguleDenominator;
    };

    // cherry_trunk and the cherry_canopy of its top and branches; the inclusive ranges keep their maximum
    struct Cherry {
        int32_t mBaseHeight;
        // each interval adds nextInt(interval + 1)
        std::vector<int32_t> mHeightIntervals;
        // one_branch, two_branches and two_branches_and_trunk
        int32_t mTypeWeights[3];
        Range mBranchStart;
        Range mBranchEnd;
        Range mBranchLength;
        Range mCanopyHeight;
        Range mCanopyRadius;
        float mWideBottomHoleChance;
        float mCornerHoleChance;
        float mHangingChance;
        float mHangingExtensionChance;
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
        // may_replace holds only air and the leaves tag
        bool mAirAndLeaves = false;
        // the vanilla canopy when null
        const SimpleCanopy *mSimpleCanopy = nullptr;
        const Mangrove *mMangrove = nullptr;
        const Cherry *mCherry = nullptr;
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

    // mega_canopy: core_width 0 when absent, the canopy then spans the width the trunk gives
    struct MegaCanopy {
        Range mHeight;
        int32_t mBaseRadius;
        int32_t mCoreWidth;
        bool mSimplify;
    };

    // mega_trunk with its branches and decoration, mega_pine_canopy or mega_canopy, and the base_cluster of the
    // mega spruce and pine features
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
        MegaCanopy mCanopy = {};
        // branches: none at a length of 0
        int32_t mBranchLength = 0;
        float mBranchSlope = 0.0f;
        Range mBranchInterval = {};
        float mBranchAltitudeMin = 0.0f;
        float mBranchAltitudeMax = 0.0f;
        MegaCanopy mBranchCanopy = {};
        // trunk_decoration vines, numerator in denominator; 0 for none
        uint32_t mVineNumerator = 0;
        uint32_t mVineDenominator = 0;
    };

    // acacia_trunk with its branches and their acacia_canopy, and the roofed_canopy it carries
    struct Acacia {
        int32_t mTrunkWidth;
        int32_t mBaseHeight;
        std::vector<int32_t> mHeightIntervals;
        int32_t mMinHeightForCanopy;
        Range mLeanHeight;
        Range mLeanSteps;
        Range mLeanLength;
        bool mAllowDiagonalGrowth;
        Range mBranchLength;
        Range mBranchPosition;
        uint32_t mBranchNumerator;
        uint32_t mBranchDenominator;
        int32_t mBranchCanopySize;
        bool mBranchCanopySimplified;
        // trunk_decoration vines, numerator in denominator on each outer side of every log; 0 for none
        uint32_t mVineNumerator;
        uint32_t mVineDenominator;
        // roofed_canopy: canopy_height, core_width, outer_radius and inner_radius
        int32_t mCanopyHeight;
        int32_t mCanopyCoreWidth;
        int32_t mCanopyOuterRadius;
        int32_t mCanopyInnerRadius;
        // acacia_canopy canopy_size of the diagonal trunk's top
        int32_t mCanopySize = 0;
    };

    using Decorate = std::function<void(int32_t, int32_t, int32_t)>;

    // placedAt, when given, gets the position the feature hands to the next one of a sequence: one above the top
    // log of a simple trunk, the position itself for the other trunks
    static bool place(BlockManager &blocks, const Definition &tree, const Fancy *fancy, const Mega *mega,
                      const Acacia *acacia, int32_t x, int32_t y, int32_t z, std::mt19937 &random,
                      const Decorate &decorate, int32_t *placedAt = nullptr);

    // the blocks a trunk's log may replace besides may_replace (sub_1471C66B0)
    static bool logReplaces(const Definition &tree, const BlockState &state);

private:
    static bool _canPlace(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                          int32_t height);

    static bool _simpleTrunk(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                             int32_t height, std::mt19937 &random, int32_t *placedAt);

    static bool _fancyTrunk(BlockManager &blocks, const Definition &tree, const Fancy &fancy, int32_t x, int32_t y,
                            int32_t z, int32_t height, std::mt19937 &random);

    static bool _fallenTrunk(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                             int32_t length, std::mt19937 &random, const Decorate &decorate);

    static bool _megaTrunk(BlockManager &blocks, const Definition &tree, const Mega &mega, int32_t x, int32_t y,
                           int32_t z, int32_t height, std::mt19937 &random);

    static bool _megaBranches(BlockManager &blocks, const Definition &tree, const Mega &mega, int32_t x, int32_t y,
                              int32_t z, int32_t height, std::mt19937 &random);

    static bool _megaCanopy(BlockManager &blocks, const Definition &tree, const MegaCanopy &canopy, int32_t width,
                            int32_t x, int32_t y, int32_t z, std::mt19937 &random);

    static void _baseCluster(BlockManager &blocks, const Mega &mega, int32_t x, int32_t y, int32_t z);

    static bool _acaciaTrunk(BlockManager &blocks, const Definition &tree, const Acacia &acacia, int32_t x, int32_t y,
                             int32_t z, int32_t height, std::mt19937 &random);

    static bool _mangroveRoots(BlockManager &blocks, const Definition &tree, const Mangrove &mangrove, int32_t x,
                               int32_t y, int32_t z, int32_t height, std::mt19937 &random, int32_t &trunkY);

    static bool _rootPath(BlockManager &blocks, const Mangrove &mangrove, const std::array<int32_t, 3> &from,
                          const std::array<int32_t, 3> &trunk, int32_t direction,
                          std::vector<std::array<int32_t, 3>> &path, int32_t depth, std::mt19937 &random);

    static void _mangroveTrunk(BlockManager &blocks, const Definition &tree, const Mangrove &mangrove, int32_t x,
                               int32_t y, int32_t z, int32_t height, std::mt19937 &random);

    static void _mangroveCanopy(BlockManager &blocks, const Definition &tree, const Mangrove &mangrove,
                                const std::vector<std::array<int32_t, 3>> &attachments, std::mt19937 &random);

    static void _cherryTrunk(BlockManager &blocks, const Definition &tree, const Cherry &cherry, int32_t x, int32_t y,
                             int32_t z, int32_t height, std::mt19937 &random);

    static std::array<int32_t, 3> _cherryBranch(BlockManager &blocks, const Definition &tree, const Cherry &cherry,
                                                int32_t x, int32_t y, int32_t z, int32_t height, int32_t stepX,
                                                int32_t stepZ, int32_t start, bool extend, std::mt19937 &random);

    static void _cherryCanopy(BlockManager &blocks, const Definition &tree, const Cherry &cherry, int32_t x, int32_t y,
                              int32_t z, std::mt19937 &random);

    static void _cherryRow(BlockManager &blocks, const Definition &tree, const Cherry &cherry, int32_t x, int32_t y,
                           int32_t z, int32_t offsetY, int32_t radius, bool hanging, std::mt19937 &random);

    static void _hangVine(BlockManager &blocks, int32_t x, int32_t y, int32_t z, int32_t bits, int32_t steps);

    static bool _diagonalAcaciaTrunk(BlockManager &blocks, const Definition &tree, const Acacia &acacia, int32_t x,
                                     int32_t y, int32_t z, int32_t height, std::mt19937 &random);

    static void _decorateLog(BlockManager &blocks, uint32_t numerator, uint32_t denominator, int32_t x, int32_t y,
                             int32_t z, const bool sides[4], std::mt19937 &random);

    static void _roofedCanopy(BlockManager &blocks, const Definition &tree, const Acacia &acacia, int32_t x,
                              int32_t y, int32_t z, std::mt19937 &random);

    static void _acaciaCanopy(BlockManager &blocks, const Definition &tree, int32_t size, bool simplified, int32_t x,
                              int32_t y, int32_t z);

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

    static void _hangCanopyVines(BlockManager &blocks, const Definition &tree, const SimpleCanopy &shape, int32_t x,
                                 int32_t y, int32_t z, std::mt19937 &random);

    static void _fancyLayer(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                            int32_t radius);

    static void _vines(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                       std::mt19937 &random);
};
