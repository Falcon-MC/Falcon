#pragma once

#include <cstdint>
#include <random>
#include <vector>

class BiomeFeatureEntry;
class BlockManager;
class LevelChunk;

// the underground_pass ore rules, scattered and shaped like the game's feature rules (sub_14A22D800)
class OverworldOrePlacer {
public:
    // whether apply places this rule, so the feature builder must not place it again
    static bool handles(const BiomeFeatureEntry &entry);

    // biomes in the order the game visits them; with more than one, each rule only places in its own biome
    static void apply(BlockManager &blocks, const LevelChunk &chunk, uint32_t levelSeed,
                      const std::vector<int32_t> &biomes);

private:
    struct OreDefinition;

    static const OreDefinition *_find(const BiomeFeatureEntry &entry);

    static void _placeRule(BlockManager &blocks, const LevelChunk &chunk, const BiomeFeatureEntry &entry,
                           const OreDefinition &ore, uint32_t decorationSeed, int32_t biomeFilter);

    static bool _placeOre(BlockManager &blocks, const OreDefinition &ore, int32_t x, int32_t y, int32_t z,
                          std::mt19937 &random);
};
