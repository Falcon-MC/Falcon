#pragma once

#include "World/Level/Block/Block.h"

#include <cstdint>
#include <functional>
#include <random>

class BlockManager;

// the game's minecraft:tree_feature with the trunks and canopies of the overworld's vanilla trees
// (VanillaTreeFeature, place sub_14A2C9A20); every draw comes from the placement twister it is given
class OverworldTreePlacer {
public:
    enum class Trunk {
        Simple,
        Fancy,
        Fallen
    };

    enum class Canopy {
        None,
        Simple,
        Fancy,
        Spruce,
        Pine
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

    using Decorate = std::function<void(int32_t, int32_t, int32_t)>;

    static bool place(BlockManager &blocks, const Definition &tree, const Fancy *fancy, int32_t x, int32_t y,
                      int32_t z, std::mt19937 &random, const Decorate &decorate);

private:
    static bool _canPlace(BlockManager &blocks, int32_t x, int32_t y, int32_t z, int32_t height);

    static bool _simpleTrunk(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                             int32_t height, std::mt19937 &random);

    static bool _fancyTrunk(BlockManager &blocks, const Definition &tree, const Fancy &fancy, int32_t x, int32_t y,
                            int32_t z, int32_t height, std::mt19937 &random);

    static bool _fallenTrunk(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                             int32_t length, std::mt19937 &random, const Decorate &decorate);

    static bool _canopy(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                        std::mt19937 &random);

    static bool _simpleCanopy(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                              std::mt19937 &random);

    static bool _spruceCanopy(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                              std::mt19937 &random);

    static bool _pineCanopy(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                            std::mt19937 &random);

    static void _fancyLayer(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                            int32_t radius);

    static void _vines(BlockManager &blocks, const Definition &tree, int32_t x, int32_t y, int32_t z,
                       std::mt19937 &random);
};
