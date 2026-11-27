#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"

#include "Core/Json/Json.h"
#include "World/Level/Dimension/Dimension.h"
#include "Scripting/BehaviorPackManager.h"
#include "Server/ServerPaths.h"
#include "Scripting/Content/PackBlockState.h"
#include "World/Level/LevelGen/Overworld/Biome/ClimateAttributes.h"
#include "World/Level/LevelGen/Overworld/Surface/SurfaceMaterialAttributes.h"
#include "World/Level/LevelGen/Overworld/Surface/SurfaceSimplexNoise.h"

#include <array>
#include <cmath>
#include <fstream>
#include <iterator>
#include <map>
#include <random>

#include "BiomeDefinitionsNbt.h"
#include "Core/Archive/Gzip.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/NBT/NbtIo.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/End/TheEndGeneratorConstants.h"
#include "World/Level/LevelGen/Nether/NetherBiomeIds.h"

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
                {"dappled_forest",                 BiomeIds::DAPPLED_FOREST},
                {"sulfur_caves",                   BiomeIds::SULFUR_CAVES},
                {"ice_caves",                      BiomeIds::ICE_CAVES},
                {"hell",                           NetherBiomeIds::HELL},
                {"soulsand_valley",                NetherBiomeIds::SOULSAND_VALLEY},
                {"crimson_forest",                 NetherBiomeIds::CRIMSON_FOREST},
                {"warped_forest",                  NetherBiomeIds::WARPED_FOREST},
                {"basalt_deltas",                  NetherBiomeIds::BASALT_DELTAS},
                {"the_end",                        TheEndGeneratorConstants::BIOME_ID}
        };

        return map;
    }

    BiomeFeatureEntry featureEntry(const char *identifier, const char *feature) {
        BiomeFeatureEntry entry;
        entry.mIdentifier = identifier;
        entry.mFeature = feature;
        entry.mEvalOrder = 1;
        return entry;
    }

    std::vector<BiomeFeatureEntry> iceCavesFeatures() {
        return {
                featureEntry("falcon:ice_caves_surface_feature_rules", "falcon:ice_caves_surface_feature"),
                featureEntry("minecraft:ice_caves_after_surface_ice_crystal_feature_rules",
                             "minecraft:ice_crystal_scatter_feature"),
                featureEntry("minecraft:ice_caves_after_surface_icicle_feature_rules",
                             "minecraft:icicle_cluster_feature"),
                featureEntry("minecraft:ice_caves_after_surface_large_icicle_feature_rules",
                             "minecraft:large_icicle_feature"),
                featureEntry("minecraft:ice_caves_after_surface_snow_layer_feature_rules",
                             "minecraft:snow_layer_scatter_feature")
        };
    }

    // a rule of the dappled forest's own: uniform x and z over the chunk, y on the heightmap
    BiomeFeatureEntry heightmapRule(const char *identifier, const char *feature, const char *pass,
                                    const char *iterations, int32_t numerator, int32_t denominator) {
        BiomeFeatureEntry entry;
        entry.mIdentifier = identifier;
        entry.mFeature = feature;
        entry.mPass = pass;
        entry.mEvalOrder = 1;
        entry.mCoordinates[0] = {1, "0.000000", "16.000000"};
        entry.mCoordinates[1] = {0, "query.heightmap(variable.worldx, variable.worldz)", ""};
        entry.mCoordinates[2] = {1, "0.000000", "16.000000"};
        entry.mChanceNumerator = numerator;
        entry.mChanceDenominator = denominator;
        entry.mChancePercent = "100.000000";
        entry.mIterations = iterations;
        return entry;
    }

    // the packet dump sends no generation data for it: the rules whose biome filters pass its tags
    // (vanilla_1.26.50 feature_rules), in the order every other biome keeps them; its own three come where the
    // other biomes keep theirs, between gravel and sand and after the tall grass
    std::vector<BiomeFeatureEntry> dappledForestFeatures(
            const std::unordered_map<int32_t, std::vector<BiomeFeatureEntry>> &known) {
        static const char *const SHARED[] = {
                "minecraft:overworld_after_surface_reeds_feature_rules",
                "minecraft:overworld_underwater_magma_feature",
                "minecraft:forest_after_surface_tall_grass_feature_rules",
                "",
                "minecraft:overworld_amethyst_geode_feature",
                "minecraft:overworld_underground_iron_ore_small_feature",
                "minecraft:overworld_underground_diamond_ore_feature_square",
                "minecraft:overworld_underground_coal_ore_upper_feature",
                "minecraft:overworld_underground_andesite_feature",
                "minecraft:overworld_underground_iron_ore_upper_feature",
                "minecraft:overworld_underground_gold_ore_feature",
                "minecraft:overworld_underground_diamond_ore_buried_feature",
                "minecraft:overworld_underground_andesite_lower_feature",
                "minecraft:overworld_underground_gravel_ore_feature",
                "minecraft:overworld_underground_coal_ore_feature",
                "minecraft:overworld_underground_andesite_upper_feature",
                "minecraft:overworld_underground_copper_ore_feature",
                "minecraft:overworld_underground_coal_ore_lower_feature",
                "minecraft:overworld_underground_diamond_ore_feature",
                "minecraft:overworld_underground_iron_ore_middle_feature",
                "minecraft:overworld_underground_diamond_ore_large_feature",
                "minecraft:overworld_underground_diorite_feature",
                "minecraft:overworld_underground_diorite_lower_feature",
                "minecraft:overworld_underground_diorite_upper_feature",
                "minecraft:overworld_underground_redstone_ore_lower_feature",
                "minecraft:overworld_underground_dirt_feature",
                "minecraft:overworld_underground_extra_gravel_ore_feature",
                "minecraft:overworld_underground_gold_ore_lower_feature",
                "minecraft:overworld_underground_granite_feature",
                "minecraft:overworld_underground_granite_lower_feature",
                "minecraft:overworld_underground_granite_upper_feature",
                "minecraft:overworld_underground_iron_ore_feature",
                "minecraft:overworld_underground_lapis_ore_buried_feature",
                "minecraft:overworld_underground_lapis_ore_feature",
                "minecraft:overworld_underground_redstone_ore_feature",
                "minecraft:overworld_underground_tuff_feature",
                "minecraft:overworld_surface_springs_feature",
                "minecraft:overworld_surface_gravel_feature",
                "",
                "minecraft:minecraft:overworld_surface_sand_feature",
                "minecraft:overworld_surface_clay_feature",
                "minecraft:overworld_underground_glow_lichen_feature",
                "minecraft:overworld_underground_deepslate_feature",
                "minecraft:overworld_extra_cave_carver_feature",
                "minecraft:overworld_cave_carver_feature"
        };
        std::vector<BiomeFeatureEntry> features;
        bool afterSurface = true;
        for (const char *identifier: SHARED) {
            if (*identifier == '\0') {
                if (afterSurface) {
                    features.push_back(heightmapRule(
                            "minecraft:dappled_forest_after_surface_brown_mushroom_feature_rules",
                            "minecraft:scatter_brown_mushroom_feature", "after_surface_pass", "1.000000", 1, 2));
                } else {
                    features.push_back(heightmapRule("minecraft:scatter_tree_dappled_forest_feature_rules",
                                                     "minecraft:select_tree_dappled_forest_feature", "surface_pass",
                                                     "6.000000", 0, 0));
                    features.push_back(heightmapRule("minecraft:scatter_red_shrub_bush_feature_rules",
                                                     "minecraft:scatter_red_shrub_bush_feature", "surface_pass",
                                                     "1.000000", 0, 0));
                }
                afterSurface = false;
                continue;
            }
            for (const auto &biome: known) {
                const auto found = std::find_if(biome.second.begin(), biome.second.end(),
                                                [&](const BiomeFeatureEntry &entry) {
                                                    return entry.mIdentifier == identifier;
                                                });
                if (found == biome.second.end())
                    continue;
                features.push_back(*found);
                break;
            }
        }
        return features;
    }

    std::string stripNamespace(const std::string &name) {
        const size_t separator = name.find(':');
        if (separator == std::string::npos)
            return name;

        return name.substr(separator + 1);
    }

    uint16_t stringIndex(std::vector<std::string> &strings, const std::string &value) {
        const std::vector<std::string>::const_iterator found = std::find(strings.begin(), strings.end(), value);
        if (found != strings.end())
            return (uint16_t) (found - strings.begin());

        strings.push_back(value);
        return (uint16_t) (strings.size() - 1);
    }

    struct PackBiome {
        std::optional<ClimateAttributes> mClimate;
        std::optional<SurfaceBuilderComponent> mSurface;
        std::array<std::optional<BlockState>, 5> mMaterials;
    };

    struct Replacement {
        int32_t mBiome;
        float mAmount;
        float mFrequency;
    };

    std::map<int32_t, PackBiome> packBiomes;
    std::map<std::string, int32_t> packBiomeIds;
    std::map<int32_t, std::vector<Replacement>> overworldReplacements;
    std::map<int32_t, std::vector<Replacement>> netherReplacements;

    const json::Value &member(const json::Value &parent, const char *key) {
        static const json::Value EMPTY;
        const auto *value = parent.get(key);
        return value == nullptr ? EMPTY : *value;
    }

    void readSurface(PackBiome &biome, const json::Value &components, const SurfaceBuilderComponent *original) {
        const auto &surfaceBuilder = member(components, "minecraft:surface_builder");
        const auto *builder = surfaceBuilder.get("builder");
        const auto *material = builder != nullptr ? builder : components.get("minecraft:surface_parameters");
        if (material == nullptr && original == nullptr)
            return;
        if (original != nullptr)
            biome.mSurface = *original;
        if (material == nullptr)
            return;

        if (!biome.mSurface)
            biome.mSurface.emplace();
        if (builder != nullptr) {
            const std::string type = member(*builder, "type").string("minecraft:overworld");
            if (type == "minecraft:overworld")
                biome.mSurface->mType = SurfaceBuilderType::Overworld;
            else if (type == "minecraft:mesa") {
                biome.mSurface->mType = SurfaceBuilderType::Mesa;
                biome.mSurface->mBrycePillars = member(*builder, "bryce_pillars").boolean();
                biome.mSurface->mHasForest = member(*builder, "has_forest").boolean();
            } else if (type == "minecraft:swamp") {
                biome.mSurface->mType = SurfaceBuilderType::Swamp;
                const double puddle = member(*builder, "max_puddle_depth_below_sea_level").number();
                if (puddle >= 0 && puddle <= 127)
                    biome.mSurface->mMaxPuddleDepth = (int32_t) puddle;
            } else if (type == "minecraft:frozen_ocean") {
                biome.mSurface->mType = SurfaceBuilderType::FrozenOcean;
            }
        }
        auto &surface = biome.mSurface->mSurfaceMaterial;
        const std::array<const char *, 5> keys{"top_material", "mid_material", "sea_floor_material",
                                              "foundation_material", "sea_material"};
        const std::array<const BlockState **, 5> slots{&surface.mTopBlock, &surface.mMidBlock, &surface.mSeaFloorBlock,
                                                      &surface.mFoundationBlock, &surface.mSeaBlock};
        for (size_t i = 0; i < keys.size(); ++i) {
            biome.mMaterials[i] = PackBlockState::read(member(*material, keys[i]));
            if (biome.mMaterials[i])
                *slots[i] = &*biome.mMaterials[i];
        }
        const double depth = member(*material, "sea_floor_depth").number(surface.mSeaFloorDepth);
        if (depth >= 0 && depth <= 127)
            surface.mSeaFloorDepth = (int32_t) depth;
    }

    // chunk generation data is left out: the game only sends it with client-side generation, never on here
    BiomeDefinitionData readDefinition(const Tag &data) {
        BiomeDefinitionData definition;
        if (data.contains("id", Tag::Type::Short))
            definition.mId = (uint16_t) data.get("id")->asShort();
        if (data.contains("temperature", Tag::Type::Float))
            definition.mTemperature = data.get("temperature")->asFloat();
        if (data.contains("downfall", Tag::Type::Float))
            definition.mDownfall = data.get("downfall")->asFloat();
        if (data.contains("foliageSnow", Tag::Type::Float))
            definition.mFoliageSnow = data.get("foliageSnow")->asFloat();
        if (data.contains("depth", Tag::Type::Float))
            definition.mDepth = data.get("depth")->asFloat();
        if (data.contains("scale", Tag::Type::Float))
            definition.mScale = data.get("scale")->asFloat();
        if (data.contains("mapWaterColorARGB", Tag::Type::Int))
            definition.mMapWaterColorArgb = data.get("mapWaterColorARGB")->asInt();
        if (data.contains("rain", Tag::Type::Byte))
            definition.mRain = data.get("rain")->asByte() != 0;

        return definition;
    }
}

std::unordered_map<int32_t, std::vector<BiomeFeatureEntry>> &
BiomeChunkGenDataRegistry::_featuresByBiome() {
    static std::unordered_map<int32_t, std::vector<BiomeFeatureEntry>> map;
    return map;
}

std::unordered_map<int32_t, std::unordered_set<std::string>> &BiomeChunkGenDataRegistry::_tagsByBiome() {
    static std::unordered_map<int32_t, std::unordered_set<std::string>> map;
    return map;
}

std::vector<BiomeDefinitionData> &BiomeChunkGenDataRegistry::_definitions() {
    static std::vector<BiomeDefinitionData> definitions;
    return definitions;
}

std::vector<std::string> &BiomeChunkGenDataRegistry::_definitionStrings() {
    static std::vector<std::string> strings;
    return strings;
}

const std::vector<BiomeDefinitionData> &BiomeChunkGenDataRegistry::getDefinitions() {
    return _definitions();
}

const std::vector<std::string> &BiomeChunkGenDataRegistry::getDefinitionStrings() {
    return _definitionStrings();
}

void BiomeChunkGenDataRegistry::loadPacks(const std::vector<BehaviorPack> &packs) {
    if (!loaded)
        return;
    static const auto vanillaDefinitions = _definitions();
    static const auto vanillaStrings = _definitionStrings();
    static const auto vanillaTags = _tagsByBiome();
    _definitions() = vanillaDefinitions;
    _definitionStrings() = vanillaStrings;
    _tagsByBiome() = vanillaTags;
    packBiomes.clear();
    packBiomeIds.clear();
    overworldReplacements.clear();
    netherReplacements.clear();

    std::map<std::string, std::unique_ptr<json::Value>> documents;
    for (const BehaviorPack &pack: packs) {
        pack.visitJson("biomes", [&](const json::Value &doc) {
            const auto &biome = member(doc, "minecraft:biome");
            std::string name = member(member(biome, "description"), "identifier").string();
            if (name.starts_with("minecraft:"))
                name = stripNamespace(name);
            if (!name.empty())
                documents[name] = member(biome, "components").clone();
        });
    }
    if (documents.empty())
        return;

    // chunks store the numeric ID; reserve IDs even when a pack is disabled
    const auto path = ServerPaths::resolve("custom-biome-ids.json");
    auto ids = json::Value::ofObject();
    std::unordered_set<int32_t> usedIds;
    for (const auto &[name, id]: biomeNameToId())
        usedIds.insert(id);
    for (const auto &biome: vanillaDefinitions)
        usedIds.insert(biome.mId);

    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error)
        return;
    if (exists) {
        std::ifstream file(path, std::ios::binary);
        const std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        auto saved = json::parse(data);
        if (saved == nullptr || !saved->isObject()) {
            LOG_ERROR(LogAreaID::Server, "Invalid custom biome ID map: %s", path.string().c_str());
            return;
        }
        for (const auto &key: saved->mKeys) {
            const double id = saved->mObject.at(key)->number(-1);
            if (!(id >= 0 && id < UINT16_MAX && id == std::trunc(id))
                || !usedIds.insert((int32_t) id).second) {
                LOG_ERROR(LogAreaID::Server, "Conflicting custom biome ID in %s", path.string().c_str());
                return;
            }
        }
        ids = std::move(saved);
    }

    bool changed = false;
    int32_t nextId = 0;
    for (const auto &[name, components]: documents) {
        if (getBiomeId(name) >= 0 || ids->get(name) != nullptr || name.find(':') == std::string::npos
            || name.starts_with("minecraft:"))
            continue;
        while (nextId < UINT16_MAX && usedIds.contains(nextId))
            ++nextId;
        if (nextId == UINT16_MAX)
            break;
        ids->set(name, json::Value::ofNumber(nextId));
        usedIds.insert(nextId);
        changed = true;
    }
    if (changed) {
        const std::filesystem::path temporary(path.string() + ".tmp");
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        file << json::stringify(*ids);
        file.close();
        if (file)
            std::filesystem::rename(temporary, path, error);
        if (!file || error) {
            LOG_ERROR(LogAreaID::Server, "Could not persist custom biome IDs to %s", path.string().c_str());
            std::filesystem::remove(temporary, error);
            return;
        }
    }

    for (const auto &[name, definition]: documents) {
        const int32_t vanillaId = getBiomeId(name);
        const auto *savedId = ids->get(name);
        const int32_t id = vanillaId >= 0 ? vanillaId : savedId != nullptr ? savedId->integer(-1) : -1;
        if (id < 0)
            continue;
        auto existing = std::find_if(_definitions().begin(), _definitions().end(), [id](const BiomeDefinitionData &biome) {
            return biome.mId == id;
        });
        const bool custom = vanillaId < 0;
        if ((!custom && existing == _definitions().end())
            || (custom && (_definitions().size() >= UINT16_MAX || _definitionStrings().size() >= UINT16_MAX)))
            continue;

        const auto &components = *definition;
        auto &replacementBiome = packBiomes[id];
        BiomeDefinitionData data = custom ? BiomeDefinitionData() : *existing;
        data.mId = (uint16_t) id;
        if (custom)
            data.mName = name;
        if (const auto *climate = components.get("minecraft:climate")) {
            ClimateAttributes &attributes = replacementBiome.mClimate.emplace();
            attributes.mTemperature = data.mTemperature =
                    (float) std::clamp(member(*climate, "temperature").number(data.mTemperature), -100.0, 100.0);
            attributes.mDownfall = data.mDownfall =
                    (float) std::clamp(member(*climate, "downfall").number(data.mDownfall), 0.0, 1.0);
            attributes.mRain = data.mRain = data.mDownfall > 0.0f;
        }
        const auto &tags = member(member(components, "minecraft:tags"), "tags");
        if (tags.isArray() && _definitionStrings().size() + tags.mArray.size() + 1 <= UINT16_MAX) {
            auto &activeTags = _tagsByBiome()[id];
            activeTags.clear();
            auto &tagIndexes = data.mTagIndexes.emplace();
            for (const auto &tag: tags.mArray) {
                if (tag->isString() && activeTags.insert(tag->mString).second)
                    tagIndexes.push_back(stringIndex(_definitionStrings(), tag->mString));
            }
        }

        const auto &replacements = member(member(components, "minecraft:replace_biomes"), "replacements");
        int32_t fallback = vanillaId;
        for (const auto &replacement: replacements.mArray) {
            const auto &rule = *replacement;
            const std::string dimension = member(rule, "dimension").string();
            const double amount = member(rule, "amount").number();
            const double frequency = member(rule, "noise_frequency_scale").number();
            if ((dimension != "minecraft:overworld" && dimension != "minecraft:nether")
                || !(amount > 0 && amount <= 1 && frequency > 0 && frequency <= 100))
                continue;
            auto &rules = dimension == "minecraft:overworld" ? overworldReplacements : netherReplacements;
            for (const auto &target: member(rule, "targets").mArray) {
                if (!target->isString() || target->mString.find(':') != std::string::npos)
                    continue;
                const int32_t original = getBiomeId(target->mString);
                if (original < 0 || original == id)
                    continue;
                rules[original].push_back({id, (float) amount, (float) frequency});
                if (fallback < 0 && dimension == "minecraft:overworld")
                    fallback = original;
            }
        }
        // the lookup is still on the vanilla table for a vanilla override
        const SurfaceBuilderComponent *base = SurfaceBuilderComponent::getForBiome(fallback);
        readSurface(replacementBiome, components, base);
        if (custom) {
            if (_definitionStrings().size() >= UINT16_MAX)
                continue;
            data.mNameIndex = stringIndex(_definitionStrings(), name);
            packBiomeIds[name] = id;
            _definitions().push_back(std::move(data));
        } else {
            *existing = std::move(data);
        }
    }
}

bool BiomeChunkGenDataRegistry::hasReplacements(DimensionType dimension) {
    return dimension == DimensionType::Nether ? !netherReplacements.empty()
                                               : dimension == DimensionType::Overworld && !overworldReplacements.empty();
}

int32_t BiomeChunkGenDataRegistry::replaceBiome(DimensionType dimension, int64_t seed, int32_t x, int32_t z,
                                                int32_t original) {
    const auto &rules = dimension == DimensionType::Nether ? netherReplacements : overworldReplacements;
    if (rules.empty())
        return original;
    const auto found = rules.find(original);
    if (found == rules.end())
        return original;
    struct ReplacementNoise {
        int64_t mSeed;
        std::mt19937 mRandom;
        SurfaceSimplexNoise mNoise;
        explicit ReplacementNoise(int64_t seed)
                : mSeed(seed), mRandom((uint32_t) seed ^ (uint32_t) ((uint64_t) seed >> 32)), mNoise(mRandom) {}
    };
    thread_local std::optional<ReplacementNoise> noise;
    for (const Replacement &rule: found->second) {
        if (rule.mAmount >= 1.0f)
            return rule.mBiome;
        if (!noise || noise->mSeed != seed)
            noise.emplace(seed);
        const float scale = rule.mFrequency / 64.0f;
        const float offset = (float) rule.mBiome * 0.25f;
        const float value = noise->mNoise.getValue((float) x * scale + offset, (float) z * scale + offset);
        if ((value + 1.0f) * 0.5f < rule.mAmount)
            return rule.mBiome;
    }
    return original;
}

const ClimateAttributes *BiomeChunkGenDataRegistry::getPackClimate(int32_t biomeId) {
    const auto found = packBiomes.find(biomeId);
    return found != packBiomes.end() && found->second.mClimate ? &*found->second.mClimate : nullptr;
}

const SurfaceBuilderComponent *BiomeChunkGenDataRegistry::getPackSurface(int32_t biomeId) {
    const auto found = packBiomes.find(biomeId);
    return found != packBiomes.end() && found->second.mSurface ? &*found->second.mSurface : nullptr;
}

bool BiomeChunkGenDataRegistry::hasTag(int32_t biomeId, const std::string &tag) {
    const auto found = _tagsByBiome().find(biomeId);
    return found != _tagsByBiome().end() && found->second.count(tag) != 0;
}

std::unordered_map<int32_t, int32_t> &BiomeChunkGenDataRegistry::_villageTypes() {
    static std::unordered_map<int32_t, int32_t> map;
    return map;
}

int32_t BiomeChunkGenDataRegistry::getVillageType(int32_t biomeId) {
    const auto found = _villageTypes().find(biomeId);
    return found != _villageTypes().end() ? found->second : 4;
}

int32_t BiomeChunkGenDataRegistry::getBiomeId(const std::string &biomeName) {
    if (const auto pack = packBiomeIds.find(biomeName); pack != packBiomeIds.end())
        return pack->second;
    if (biomeName.find(':') != std::string::npos && !biomeName.starts_with("minecraft:"))
        return -1;
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

    for (const auto &[name, id]: packBiomeIds)
        names.push_back(name);
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

        const std::string name = stringAt((int32_t) indexTag->asShort());
        const Tag *tagsTag = dataTag->get("tags");
        const Tag *tagList = tagsTag == nullptr ? nullptr : tagsTag->get("tags");

        BiomeDefinitionData definition = readDefinition(*dataTag);
        // the game puts a biome's tags in the string list before its name
        if (tagList != nullptr) {
            std::vector<uint16_t> tagIndexes;
            for (const Tag &tag: tagList->getList())
                tagIndexes.push_back(stringIndex(_definitionStrings(), stringAt((int32_t) tag.asShort())));
            definition.mTagIndexes = tagIndexes;
        }
        definition.mName = name;
        definition.mNameIndex = stringIndex(_definitionStrings(), name);
        _definitions().push_back(definition);

        const int32_t biomeId = getBiomeId(name);
        if (biomeId < 0)
            continue;

        if (tagList != nullptr) {
            std::unordered_set<std::string> &tags = _tagsByBiome()[biomeId];
            for (const Tag &tag: tagList->getList())
                tags.insert(stringAt((int32_t) tag.asShort()));
        }

        const Tag *chunkGenDataTag = dataTag->get("chunkGenData");
        if (chunkGenDataTag == nullptr)
            continue;

        const Tag *villageTypeTag = chunkGenDataTag->get("villageType");
        if (villageTypeTag != nullptr)
            _villageTypes()[biomeId] = villageTypeTag->asInt();

        const Tag *consolidatedTag = chunkGenDataTag->get("consolidatedFeatures");
        if (consolidatedTag == nullptr)
            continue;

        const Tag *featuresTag = consolidatedTag->get("features");
        if (featuresTag == nullptr)
            continue;

        std::vector<BiomeFeatureEntry> features;
        for (const Tag &featureTag: featuresTag->getList()) {
            const Tag *identifierTag = featureTag.get("identifier");
            const Tag *featureNameTag = featureTag.get("feature");
            const Tag *scatterTag = featureTag.get("scatter");
            if (identifierTag == nullptr || featureNameTag == nullptr)
                continue;

            BiomeFeatureEntry data;
            data.mIdentifier = stringAt((int32_t) identifierTag->asShort());
            data.mFeature = stringAt((int32_t) featureNameTag->asShort());

            const Tag *passTag = featureTag.get("pass");
            if (passTag != nullptr)
                data.mPass = stringAt((int32_t) passTag->asShort());

            if (scatterTag != nullptr) {
                const Tag *evalOrderTag = scatterTag->get("evalOrder");
                if (evalOrderTag != nullptr)
                    data.mEvalOrder = evalOrderTag->asInt();

                const Tag *coordinatesTag = scatterTag->get("coordinates");
                if (coordinatesTag != nullptr) {
                    size_t axis = 0;
                    for (const Tag &coordinateTag: coordinatesTag->getList()) {
                        if (axis >= 3)
                            break;

                        BiomeFeatureEntry::Coordinate &coordinate = data.mCoordinates[axis++];
                        const Tag *distributionTag = coordinateTag.get("distribution");
                        const Tag *minTag = coordinateTag.get("minValue");
                        const Tag *maxTag = coordinateTag.get("maxValue");
                        if (distributionTag != nullptr)
                            coordinate.mDistribution = distributionTag->asInt();
                        if (minTag != nullptr)
                            coordinate.mMin = stringAt((int32_t) minTag->asShort());
                        if (maxTag != nullptr)
                            coordinate.mMax = stringAt((int32_t) maxTag->asShort());
                    }
                }

                const Tag *numeratorTag = scatterTag->get("chanceNumerator");
                const Tag *denominatorTag = scatterTag->get("chanceDenominator");
                const Tag *percentTag = scatterTag->get("chancePercent");
                const Tag *iterationsTag = scatterTag->get("iterations");
                if (numeratorTag != nullptr)
                    data.mChanceNumerator = numeratorTag->asInt();
                if (denominatorTag != nullptr)
                    data.mChanceDenominator = denominatorTag->asInt();
                if (percentTag != nullptr)
                    data.mChancePercent = stringAt((int32_t) percentTag->asShort());
                if (iterationsTag != nullptr)
                    data.mIterations = stringAt((int32_t) iterationsTag->asShort());
            }

            features.push_back(data);
        }

        _featuresByBiome()[biomeId] = features;
    }

    if (_featuresByBiome().find(BiomeIds::ICE_CAVES) == _featuresByBiome().end())
        _featuresByBiome()[BiomeIds::ICE_CAVES] = iceCavesFeatures();
    if (_featuresByBiome().find(BiomeIds::DAPPLED_FOREST) == _featuresByBiome().end())
        _featuresByBiome()[BiomeIds::DAPPLED_FOREST] = dappledForestFeatures(_featuresByBiome());

    auto &valley = _featuresByBiome()[NetherBiomeIds::SOULSAND_VALLEY];
    const char *const fossil = "minecraft:nether_fossil_feature";
    if (std::none_of(valley.begin(), valley.end(), [fossil](const BiomeFeatureEntry &entry) {
            return entry.mFeature == fossil;
        })) {
        auto rule = heightmapRule("minecraft:soulsand_valley_fossil_surface_feature", fossil, "surface_pass", "1", 1, 2);
        rule.mCoordinates[1] = {1, "32.000000", "125.000000"};
        valley.push_back(std::move(rule));
    }

    loaded = true;
}

const std::vector<BiomeFeatureEntry> *
BiomeChunkGenDataRegistry::getConsolidatedFeatures(int32_t biomeId) {
    const std::unordered_map<int32_t, std::vector<BiomeFeatureEntry>> &map = _featuresByBiome();
    const std::unordered_map<int32_t, std::vector<BiomeFeatureEntry>>::const_iterator found =
            map.find(biomeId);
    if (found == map.end())
        return nullptr;

    return &found->second;
}
