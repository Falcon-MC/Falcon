#pragma once

#include "Protocol/Types/BiomeDefinitionData.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class BiomeFeatureEntry {
public:
    // one axis of the rule's scatter; bounds are the game's expression strings
    struct Coordinate {
        int32_t mDistribution = 0;
        std::string mMin;
        std::string mMax;
    };

    std::string mIdentifier;
    std::string mFeature;
    std::string mPass;
    // the order the scatter evaluates x, y and z in
    int32_t mEvalOrder = 0;
    Coordinate mCoordinates[3];
    int32_t mChanceNumerator = 0;
    int32_t mChanceDenominator = 0;
    std::string mChancePercent;
    std::string mIterations;
};

class BiomeChunkGenDataRegistry {
public:
    static void initialize();

    static const std::vector<BiomeFeatureEntry> *getConsolidatedFeatures(int32_t biomeId);

    static bool isLoaded();

    static int32_t getBiomeId(const std::string &biomeName);

    static std::vector<std::string> getBiomeNames();

    static bool hasTag(int32_t biomeId, const std::string &tag);

    // every biome as the game sends it to clients; names and tags index getDefinitionStrings()
    static const std::vector<BiomeDefinitionData> &getDefinitions();

    static const std::vector<std::string> &getDefinitionStrings();

private:
    static std::unordered_map<int32_t, std::vector<BiomeFeatureEntry>> &_featuresByBiome();

    static std::unordered_map<int32_t, std::unordered_set<std::string>> &_tagsByBiome();

    static std::vector<BiomeDefinitionData> &_definitions();

    static std::vector<std::string> &_definitionStrings();
};
