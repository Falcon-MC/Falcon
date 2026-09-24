#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class BiomeFeatureEntry {
public:
    std::string mIdentifier;
    std::string mFeature;
    int32_t mEvalOrder = 0;
};

class BiomeChunkGenDataRegistry {
public:
    static void initialize();

    static const std::vector<BiomeFeatureEntry> *getConsolidatedFeatures(int32_t biomeId);

    static bool isLoaded();

    static int32_t getBiomeId(const std::string &biomeName);

    static std::vector<std::string> getBiomeNames();

private:
    static std::unordered_map<int32_t, std::vector<BiomeFeatureEntry>> &_featuresByBiome();
};
