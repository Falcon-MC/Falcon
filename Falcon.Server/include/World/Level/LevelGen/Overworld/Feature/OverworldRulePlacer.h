#pragma once

#include <cstdint>
#include <random>
#include <vector>

class BiomeFeatureEntry;
class BlockManager;
class LevelChunk;

// the feature rules whose feature is ported, scattered and shaped like the game's feature rules (sub_14A22D800)
class OverworldRulePlacer {
public:
    // whether apply places this rule, so the feature builder must not place it again
    static bool handles(const BiomeFeatureEntry &entry);

    // pass by pass, biomes in the order the game visits them; with more than one, each rule only places in its own
    // biome
    static void apply(BlockManager &blocks, const LevelChunk &chunk, uint32_t levelSeed,
                      const std::vector<int32_t> &biomes);

private:
    struct OreDefinition;
    struct DiskDefinition;
    struct SeagrassDefinition;
    struct ScatterDefinition;

    struct Feature {
        const OreDefinition *mOre = nullptr;
        const DiskDefinition *mDisk = nullptr;
        const ScatterDefinition *mScatter = nullptr;
        bool mKelp = false;

        bool isPorted() const {
            return mOre != nullptr || mDisk != nullptr || mScatter != nullptr || mKelp;
        }
    };

    static Feature _find(const BiomeFeatureEntry &entry);

    static void _placeRule(BlockManager &blocks, const LevelChunk &chunk, const BiomeFeatureEntry &entry,
                           const Feature &feature, uint32_t decorationSeed, int32_t biomeFilter);

    static bool _placeOre(BlockManager &blocks, const OreDefinition &ore, int32_t x, int32_t y, int32_t z,
                          std::mt19937 &random);

    static bool _placeDisk(BlockManager &blocks, const DiskDefinition &disk, int32_t x, int32_t y, int32_t z,
                           std::mt19937 &random);

    static bool _placeScatter(BlockManager &blocks, const ScatterDefinition &scatter, int32_t x, int32_t y, int32_t z,
                              std::mt19937 &random);

    static bool _placeSeagrass(BlockManager &blocks, const SeagrassDefinition &seagrass, int32_t x, int32_t y,
                               int32_t z, std::mt19937 &random);

    static bool _placeKelp(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random);
};
