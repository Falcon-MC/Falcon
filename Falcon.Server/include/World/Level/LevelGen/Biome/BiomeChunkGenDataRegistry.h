#pragma once

#include "Protocol/Types/BiomeDefinitionData.h"
#include "World/Level/LevelGen/Overworld/Surface/NoiseGradientSurfaceBuilder.h"

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct BehaviorPack;
enum class DimensionType : int32_t;
class ClimateAttributes;
class SurfaceBuilderComponent;

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

    static void loadPacks(const std::vector<BehaviorPack> &packs);

    static void buildPackGradients(int64_t seed, std::map<int32_t, NoiseGradientSurfaceBuilder> &surface,
                                   std::map<int32_t, NoiseGradientSurfaceBuilder> &subsurface);

    static bool hasReplacements(DimensionType dimension);

    static int32_t replaceBiome(DimensionType dimension, int64_t seed, int32_t x, int32_t z, int32_t original);

    static const ClimateAttributes *getPackClimate(int32_t biomeId);

    static const SurfaceBuilderComponent *getPackSurface(int32_t biomeId);

    static const std::vector<BiomeFeatureEntry> *getConsolidatedFeatures(int32_t biomeId);

    static bool isLoaded();

    static int32_t getBiomeId(const std::string &biomeName);

    static std::vector<std::string> getBiomeNames();

    static bool hasTag(int32_t biomeId, const std::string &tag);

    // chunkGenData.villageType: 0 desert, 1 snowy, 2 savanna, 3 taiga; plains when absent
    static int32_t getVillageType(int32_t biomeId);

    // every biome as the game sends it to clients; names and tags index getDefinitionStrings()
    static const std::vector<BiomeDefinitionData> &getDefinitions();

    static const std::vector<std::string> &getDefinitionStrings();

private:
    static std::unordered_map<int32_t, std::vector<BiomeFeatureEntry>> &_featuresByBiome();

    static std::unordered_map<int32_t, std::unordered_set<std::string>> &_tagsByBiome();

    static std::unordered_map<int32_t, int32_t> &_villageTypes();

    static std::vector<BiomeDefinitionData> &_definitions();

    static std::vector<std::string> &_definitionStrings();
};
