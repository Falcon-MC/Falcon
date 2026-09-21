#include "Level/Generator/Biome/BiomeChunkGenDataRegistry.h"

#include "BiomeDefinitionsNbt.h"
#include "Core/Archive/Gzip.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/NBT/NbtIo.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Level/Generator/Biome/BiomeIds.h"

#include <algorithm>

namespace {
    bool loaded = false;

    const std::unordered_map<std::string, int32_t> &biomeNameToId() {
        static const std::unordered_map<std::string, int32_t> map = {
                {"ocean",                          BiomeIds::OCEAN},
                {"plains",                         BiomeIds::PLAINS},
                {"desert",                         BiomeIds::DESERT},
                {"extreme_hills",                  BiomeIds::EXTREME_HILLS},
                {"forest",                         BiomeIds::FOREST},
                {"taiga",                          BiomeIds::TAIGA},
                {"swampland",                      BiomeIds::SWAMPLAND},
                {"river",                          BiomeIds::RIVER},
                {"legacy_frozen_ocean",            BiomeIds::LEGACY_FROZEN_OCEAN},
                {"frozen_river",                   BiomeIds::FROZEN_RIVER},
                {"ice_plains",                     BiomeIds::ICE_PLAINS},
                {"mushroom_island",                BiomeIds::MUSHROOM_ISLAND},
                {"beach",                          BiomeIds::BEACH},
                {"jungle",                         BiomeIds::JUNGLE},
                {"jungle_edge",                    BiomeIds::JUNGLE_EDGE},
                {"deep_ocean",                     BiomeIds::DEEP_OCEAN},
                {"stone_beach",                    BiomeIds::STONE_BEACH},
                {"cold_beach",                     BiomeIds::COLD_BEACH},
                {"birch_forest",                   BiomeIds::BIRCH_FOREST},
                {"roofed_forest",                  BiomeIds::ROOFED_FOREST},
                {"cold_taiga",                     BiomeIds::COLD_TAIGA},
                {"mega_taiga",                     BiomeIds::MEGA_TAIGA},
                {"extreme_hills_plus_trees",       BiomeIds::EXTREME_HILLS_PLUS_TREES},
                {"savanna",                        BiomeIds::SAVANNA},
                {"savanna_plateau",                BiomeIds::SAVANNA_PLATEAU},
                {"mesa",                           BiomeIds::MESA},
                {"mesa_plateau_stone",             BiomeIds::MESA_PLATEAU_STONE},
                {"warm_ocean",                     BiomeIds::WARM_OCEAN},
                {"lukewarm_ocean",                 BiomeIds::LUKEWARM_OCEAN},
                {"deep_lukewarm_ocean",            BiomeIds::DEEP_LUKEWARM_OCEAN},
                {"cold_ocean",                     BiomeIds::COLD_OCEAN},
                {"deep_cold_ocean",                BiomeIds::DEEP_COLD_OCEAN},
                {"frozen_ocean",                   BiomeIds::FROZEN_OCEAN},
                {"deep_frozen_ocean",              BiomeIds::DEEP_FROZEN_OCEAN},
                {"bamboo_jungle",                  BiomeIds::BAMBOO_JUNGLE},
                {"sunflower_plains",               BiomeIds::SUNFLOWER_PLAINS},
                {"extreme_hills_mutated",          BiomeIds::EXTREME_HILLS_MUTATED},
                {"flower_forest",                  BiomeIds::FLOWER_FOREST},
                {"ice_plains_spikes",              BiomeIds::ICE_PLAINS_SPIKES},
                {"birch_forest_mutated",           BiomeIds::BIRCH_FOREST_MUTATED},
                {"redwood_taiga_mutated",          BiomeIds::REDWOOD_TAIGA_MUTATED},
                {"savanna_mutated",                BiomeIds::SAVANNA_MUTATED},
                {"mesa_bryce",                     BiomeIds::MESA_BRYCE},
                {"jagged_peaks",                   BiomeIds::JAGGED_PEAKS},
                {"frozen_peaks",                   BiomeIds::FROZEN_PEAKS},
                {"snowy_slopes",                   BiomeIds::SNOWY_SLOPES},
                {"grove",                          BiomeIds::GROVE},
                {"meadow",                         BiomeIds::MEADOW},
                {"lush_caves",                     BiomeIds::LUSH_CAVES},
                {"dripstone_caves",                BiomeIds::DRIPSTONE_CAVES},
                {"stony_peaks",                    BiomeIds::STONY_PEAKS},
                {"deep_dark",                      BiomeIds::DEEP_DARK},
                {"mangrove_swamp",                 BiomeIds::MANGROVE_SWAMP},
                {"cherry_grove",                   BiomeIds::CHERRY_GROVE},
                {"pale_garden",                    BiomeIds::PALE_GARDEN},
                {"sulfur_caves",                   BiomeIds::SULFUR_CAVES}
        };

        return map;
    }

    std::string stripNamespace(const std::string &name) {
        const size_t separator = name.find(':');
        if (separator == std::string::npos)
            return name;

        return name.substr(separator + 1);
    }
}

std::unordered_map<int32_t, std::vector<BiomeConsolidatedFeatureData>> &
BiomeChunkGenDataRegistry::_featuresByBiome() {
    static std::unordered_map<int32_t, std::vector<BiomeConsolidatedFeatureData>> map;
    return map;
}

int32_t BiomeChunkGenDataRegistry::getBiomeId(const std::string &biomeName) {
    const std::unordered_map<std::string, int32_t> &map = biomeNameToId();
    const std::unordered_map<std::string, int32_t>::const_iterator found = map.find(stripNamespace(biomeName));
    if (found == map.end())
        return -1;

    return found->second;
}

std::vector<std::string> BiomeChunkGenDataRegistry::getBiomeNames() {
    std::vector<std::string> names;
    for (const std::pair<const std::string, int32_t> &entry: biomeNameToId())
        names.push_back(entry.first);

    std::sort(names.begin(), names.end());
    return names;
}

bool BiomeChunkGenDataRegistry::isLoaded() {
    return loaded;
}

void BiomeChunkGenDataRegistry::initialize() {
    if (loaded)
        return;

    const std::string compressed((const char *) FalconBiomeData::kBiomeDefinitionsNbt,
                                 FalconBiomeData::kBiomeDefinitionsNbtSize);

    std::string decompressed;
    if (!Gzip::decompress(compressed, decompressed)) {
        LOG_WARN(LogAreaID::Server, "Failed to decompress embedded biome definitions");
        return;
    }

    ReadOnlyBinaryStream stream(decompressed);

    Tag root;
    try {
        root = NbtIo::readTag(stream, NbtVariant::BigEndian);
    } catch (const std::exception &exception) {
        LOG_WARN(LogAreaID::Server, "Failed to parse biome definitions: %s", exception.what());
        return;
    }

    const Tag *stringListTag = root.get("biomeStringList");
    const Tag *biomeDataTag = root.get("biomeData");
    if (stringListTag == nullptr || biomeDataTag == nullptr) {
        LOG_WARN(LogAreaID::Server, "Embedded biome definitions are missing biomeStringList or biomeData");
        return;
    }

    std::vector<std::string> stringList;
    for (const Tag &entry: stringListTag->getList())
        stringList.push_back(entry.asString());

    const auto stringAt = [&stringList](int32_t index) -> std::string {
        if (index < 0 || (size_t) index >= stringList.size())
            return std::string();

        return stringList[(size_t) index];
    };

    for (const Tag &entry: biomeDataTag->getList()) {
        const Tag *indexTag = entry.get("index");
        const Tag *dataTag = entry.get("data");
        if (indexTag == nullptr || dataTag == nullptr)
            continue;

        const int32_t biomeId = getBiomeId(stringAt((int32_t) indexTag->asShort()));
        if (biomeId < 0)
            continue;

        const Tag *chunkGenDataTag = dataTag->get("chunkGenData");
        if (chunkGenDataTag == nullptr)
            continue;

        const Tag *consolidatedTag = chunkGenDataTag->get("consolidatedFeatures");
        if (consolidatedTag == nullptr)
            continue;

        const Tag *featuresTag = consolidatedTag->get("features");
        if (featuresTag == nullptr)
            continue;

        std::vector<BiomeConsolidatedFeatureData> features;
        for (const Tag &featureTag: featuresTag->getList()) {
            const Tag *identifierTag = featureTag.get("identifier");
            const Tag *featureNameTag = featureTag.get("feature");
            const Tag *scatterTag = featureTag.get("scatter");
            if (identifierTag == nullptr || featureNameTag == nullptr)
                continue;

            BiomeConsolidatedFeatureData data;
            data.mIdentifier = stringAt((int32_t) identifierTag->asShort());
            data.mFeature = stringAt((int32_t) featureNameTag->asShort());

            if (scatterTag != nullptr) {
                const Tag *evalOrderTag = scatterTag->get("evalOrder");
                if (evalOrderTag != nullptr)
                    data.mEvalOrder = evalOrderTag->asInt();
            }

            features.push_back(data);
        }

        _featuresByBiome()[biomeId] = features;
    }

    loaded = true;
}

const std::vector<BiomeConsolidatedFeatureData> *
BiomeChunkGenDataRegistry::getConsolidatedFeatures(int32_t biomeId) {
    const std::unordered_map<int32_t, std::vector<BiomeConsolidatedFeatureData>> &map = _featuresByBiome();
    const std::unordered_map<int32_t, std::vector<BiomeConsolidatedFeatureData>>::const_iterator found =
            map.find(biomeId);
    if (found == map.end())
        return nullptr;

    return &found->second;
}
