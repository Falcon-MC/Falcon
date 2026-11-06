#pragma once

#include "World/Level/Block/Block.h"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

class BiomeFeatureEntry;
class BlockManager;
class LevelChunk;
class OverworldClimate;

// the feature rules whose feature is ported, scattered and shaped like the game's feature rules (sub_14A22D800)
class OverworldRulePlacer {
public:
    // whether apply places this rule, so the feature builder must not place it again
    static bool handles(const BiomeFeatureEntry &entry);

    // pass by pass, biomes in the order the game visits them; with more than one, each rule only places in its own
    // biome
    static void apply(BlockManager &blocks, const LevelChunk &chunk, const OverworldClimate &climate,
                      uint32_t levelSeed, const std::vector<int32_t> &biomes);

    // every pass of one biome's rules, for the generators without a climate that decorate with a single biome
    static void applyBiome(BlockManager &blocks, const LevelChunk &chunk, uint32_t levelSeed, int32_t biome);

    // one vein of OreFeature for the generators that place their ores themselves
    static bool placeOreVein(BlockManager &blocks, int32_t count, Block (*places)(),
                             const std::vector<std::string> &mayReplace, int32_t x, int32_t y, int32_t z,
                             std::mt19937 &random);

private:
    struct OreDefinition;
    struct DiskDefinition;
    struct GeodeDefinition;
    struct SeagrassDefinition;
    struct ScatterDefinition;
    struct SingleBlockDefinition;
    struct WeightedDefinition;
    struct AggregateDefinition;
    struct TreeDefinition;
    struct LegacyTreeDefinition;
    struct ThresholdDefinition;
    struct SnapDefinition;
    struct BlobDefinition;
    struct MultifaceDefinition;
    struct LegacyFlowerDefinition;

    // one of the game's features a rule or another feature places
    struct FossilDefinition;

    struct Placed {
        const SeagrassDefinition *mSeagrass = nullptr;
        const ScatterDefinition *mScatter = nullptr;
        const SingleBlockDefinition *mSingleBlock = nullptr;
        const WeightedDefinition *mWeighted = nullptr;
        const AggregateDefinition *mAggregate = nullptr;
        const TreeDefinition *mTree = nullptr;
        // the game's own leaf litter feature
        bool mLeafLitter = false;
        // the game's own brown and red mushroom features
        Block (*mMushroom)() = nullptr;
        // beehive_search_feature
        bool mBeehiveSearch = false;
        const ThresholdDefinition *mThreshold = nullptr;
        const SnapDefinition *mSnap = nullptr;
        const BlobDefinition *mBlob = nullptr;
        const MultifaceDefinition *mMultiface = nullptr;
        // the game's own dripstone_cluster_feature, pointed_dripstone_feature and large_dripstone_feature
        bool mDripstoneCluster = false;
        bool mPointedDripstone = false;
        bool mLargeDripstone = false;
        const LegacyFlowerDefinition *mLegacyFlower = nullptr;
        // a feature ported on its own, placed at the position with the rule's random
        bool (*mFunction)(BlockManager &, int32_t, int32_t, int32_t, std::mt19937 &) = nullptr;

        bool isSet() const {
            return mSeagrass != nullptr || mScatter != nullptr || mSingleBlock != nullptr || mWeighted != nullptr
                   || mAggregate != nullptr || mTree != nullptr || mLeafLitter || mMushroom != nullptr
                   || mBeehiveSearch || mThreshold != nullptr || mSnap != nullptr || mBlob != nullptr
                   || mMultiface != nullptr || mDripstoneCluster || mPointedDripstone || mLargeDripstone
                   || mLegacyFlower != nullptr || mFunction != nullptr;
        }
    };

    struct Feature {
        const OreDefinition *mOre = nullptr;
        const DiskDefinition *mDisk = nullptr;
        const GeodeDefinition *mGeode = nullptr;
        const LegacyTreeDefinition *mLegacyTree = nullptr;
        Placed mPlaced;
        bool mKelp = false;
        // legacy:forest_foliage_feature
        bool mForestFoliage = false;
        // legacy:springs_feature
        bool mSprings = false;
        // legacy:ice_feature
        bool mLegacyIce = false;
        const FossilDefinition *mFossil = nullptr;

        bool isPorted() const {
            return mOre != nullptr || mDisk != nullptr || mGeode != nullptr || mLegacyTree != nullptr
                   || mPlaced.isSet() || mKelp || mForestFoliage || mSprings || mLegacyIce || mFossil != nullptr;
        }
    };

    static Feature _find(const BiomeFeatureEntry &entry);

    static void _apply(BlockManager &blocks, const LevelChunk &chunk, const OverworldClimate *climate,
                       uint32_t levelSeed, const std::vector<int32_t> &biomes);

    static void _drawUnported(BlockManager &blocks, const LevelChunk &chunk, const BiomeFeatureEntry &entry,
                              uint32_t decorationSeed);

    static void _placeRule(BlockManager &blocks, const LevelChunk &chunk, const BiomeFeatureEntry &entry,
                           const Feature &feature, uint32_t decorationSeed, int32_t biomeFilter);

    static bool _placeOre(BlockManager &blocks, const OreDefinition &ore, int32_t x, int32_t y, int32_t z,
                          std::mt19937 &random);

    static bool _placeDisk(BlockManager &blocks, const DiskDefinition &disk, int32_t x, int32_t y, int32_t z,
                           std::mt19937 &random);

    static bool _placeGeode(BlockManager &blocks, const GeodeDefinition &geode, int32_t x, int32_t y, int32_t z,
                            std::mt19937 &random);

    static void _placeLegacyTrees(BlockManager &blocks, const LegacyTreeDefinition &trees, int32_t x, int32_t z,
                                  std::mt19937 &random);

    static void _placeSprings(BlockManager &blocks, int32_t x, int32_t z, std::mt19937 &random);

    static void _placeSpring(BlockManager &blocks, int32_t x, int32_t y, int32_t z, const BlockState &liquid);

    static void _placeLegacyIce(BlockManager &blocks, int32_t x, int32_t z, std::mt19937 &random);

    static bool _placeFossil(BlockManager &blocks, const FossilDefinition &fossil, int32_t x, int32_t y, int32_t z,
                             std::mt19937 &random, uint32_t integritySeed);

    static void _placeIceberg(BlockManager &blocks, int32_t x, int32_t z, std::mt19937 &random);

    static bool _placeBlueIce(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);

    static void _placeForestFoliage(BlockManager &blocks, int32_t x, int32_t z, std::mt19937 &random);

    static void _placeLegacyDoublePlant(BlockManager &blocks, Block (*plant)(), int32_t x, int32_t y, int32_t z,
                                        std::mt19937 &random);

    static bool _placeAggregate(BlockManager &blocks, const AggregateDefinition &aggregate, int32_t x, int32_t y,
                                int32_t z, std::mt19937 &random);

    static bool _placeTree(BlockManager &blocks, const TreeDefinition &tree, int32_t x, int32_t y, int32_t z,
                           std::mt19937 &random);

    static bool _placeLeafLitter(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);

    static bool _placeMushroom(BlockManager &blocks, Block (*mushroom)(), int32_t x, int32_t y, int32_t z);

    static bool _placeBeehiveSearch(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);

    static bool _place(BlockManager &blocks, const Placed &placed, int32_t x, int32_t y, int32_t z,
                       std::mt19937 &random);

    static bool _placeScatter(BlockManager &blocks, const ScatterDefinition &scatter, int32_t x, int32_t y, int32_t z,
                              std::mt19937 &random);

    static bool _placeSeagrass(BlockManager &blocks, const SeagrassDefinition &seagrass, int32_t x, int32_t y,
                               int32_t z, std::mt19937 &random);

    static bool _placeSingleBlock(BlockManager &blocks, const SingleBlockDefinition &single, int32_t x, int32_t y,
                                  int32_t z, std::mt19937 &random);

    static bool _placeWeighted(BlockManager &blocks, const WeightedDefinition &weighted, int32_t x, int32_t y,
                               int32_t z, std::mt19937 &random);

    static bool _placeThreshold(BlockManager &blocks, const ThresholdDefinition &threshold, int32_t x, int32_t y,
                                int32_t z, std::mt19937 &random);

    static bool _placeSnap(BlockManager &blocks, const SnapDefinition &snap, int32_t x, int32_t y, int32_t z,
                           std::mt19937 &random);

    static bool _snapAlong(BlockManager &blocks, const SnapDefinition &snap, int32_t x, int32_t y, int32_t z,
                           int32_t face, int32_t snapped[3]);

    static bool _placeBlob(BlockManager &blocks, const BlobDefinition &blob, int32_t x, int32_t y, int32_t z,
                           std::mt19937 &random);

    static bool _placeMultiface(BlockManager &blocks, const MultifaceDefinition &multiface, int32_t x, int32_t y,
                                int32_t z, std::mt19937 &random);

    static bool _growMultiface(BlockManager &blocks, const MultifaceDefinition &multiface, int32_t x, int32_t y,
                               int32_t z, const std::vector<int32_t> &faces, std::mt19937 &random);

    static bool _placeDripstoneCluster(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);

    static bool _placePointedDripstone(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);

    static bool _placeLargeDripstone(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);

    static bool _placeLegacyFlower(BlockManager &blocks, const LegacyFlowerDefinition &flower, int32_t x, int32_t y,
                                   int32_t z, std::mt19937 &random);

    static bool _placeKelp(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);
};
