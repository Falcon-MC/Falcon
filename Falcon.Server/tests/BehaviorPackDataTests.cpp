#include "Network/ServerNetworkHandler.h"
#include "Scripting/BehaviorPackManager.h"
#include "Server/CameraPresetRegistry.h"
#include "Server/Localization.h"
#include "Server/ServerHost.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Spawn/SpawnRules.h"
#include "World/Level/LevelGen/DimensionFactory.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/GeneratorChunkSource.h"
#include "World/Level/LevelGen/Structure/JigsawStructures.h"
#include "World/Level/Structure/McStructure.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void checkPackSpawnRules(const std::filesystem::path &directory) {
        const std::filesystem::path pack = directory / "behavior_packs" / "spawn-override";
        const std::filesystem::path rules = pack / "spawn_rules";
        std::filesystem::create_directories(rules / "nested");
        std::ofstream(pack / "manifest.json") << R"({
            "format_version": 2,
            "header": {"name": "Spawn override", "uuid": "cce1a513-d5ab-4e05-ada2-9b57f39651ef", "version": [1, 0, 0]},
            "modules": [{"type": "data", "uuid": "edf0d31a-f3f3-41bd-96b0-e44319980b26", "version": [1, 0, 0]}]
        })";
        std::ofstream(rules / "nested" / "zombie.json") << R"({
            "format_version": "1.8.0",
            "minecraft:spawn_rules": {
                "description": {"identifier": "minecraft:zombie", "population_control": "monster"},
                "conditions": [{"minecraft:spawns_on_surface": {}, "minecraft:weight": {"default": 12}}]
            }
        })";
        std::ofstream(rules / "invalid.json") << R"({"minecraft:spawn_rules": {"conditions": [null]}})";
        BehaviorPackManager packs;
        packs.discover((directory / "behavior_packs").string());
        require(packs.getPackCount() == 1, "a pack containing only spawn rules was not discovered");
        SpawnRules::addPack(packs.getPacks().front().mPath);

        std::filesystem::create_directories(pack / "texts");
        std::ofstream(pack / "texts" / "en_US.lang") << "\xEF\xBB\xBF" "pack.test.greeting=Hello %s\t# comment\n";
        std::ofstream(pack / "texts" / "fr_FR.lang") << "pack.test.greeting=Bonjour %s\n";

        Localization &localization = Localization::getInstance();
        localization.loadPackTexts(packs.getPacks());
        require(localization.translate("fr_FR", "pack.test.greeting", {"Alex"}) == "Bonjour Alex",
                "the pack's French translation was not selected");
        require(localization.translate("es_ES", "pack.test.greeting", {"Alex"}) == "Hello Alex",
                "the pack's English fallback was not selected");
        localization.loadPackTexts({});
        require(localization.translate("fr_FR", "pack.test.greeting", {}) == "pack.test.greeting",
                "translations from an unloaded pack survived");

        const auto cameras = pack / "cameras" / "presets";
        std::filesystem::create_directories(cameras);
        std::ofstream(cameras / "child.json") << R"({
            "minecraft:camera_preset": {
                "identifier": "example:child", "inherit_from": "example:parent", "pos_x": 18
            }
        })";
        std::ofstream(cameras / "parent.json") << R"({
            "minecraft:camera_preset": {
                "identifier": "example:parent", "inherit_from": "minecraft:free", "pos_y": 70, "rot_x": 22
            }
        })";
        std::ofstream(cameras / "invalid.json") << R"({
            "minecraft:camera_preset": {"identifier": "example:cycle", "inherit_from": "example:cycle"}
        })";
        CameraPresetRegistry::load(packs.getPacks());
        const int parent = CameraPresetRegistry::runtimeId("example:parent");
        const int child = CameraPresetRegistry::runtimeId("example:child");
        require(parent >= 0 && parent < child, "camera presets were not registered in inheritance order");
        const CameraPreset &camera = CameraPresetRegistry::getPresets()[(size_t) child];
        require(camera.mParentPreset == "example:parent" && !camera.mHasPos && camera.mHasPosX
                        && !camera.mHasPosY && !camera.mHasPosZ && camera.mPos.x == 18.0f,
                "a partial camera position lost its inheritance");
        require(CameraPresetRegistry::runtimeId("example:cycle") < 0, "cyclic camera preset was registered");
        CameraPresetRegistry::load({});
        require(CameraPresetRegistry::runtimeId("example:child") < 0, "camera presets from an unloaded pack survived");

        const auto &registered = SpawnRules::getRules();
        const auto isZombie = [](const SpawnRule &rule) { return rule.mIdentifier == "minecraft:zombie"; };
        require(std::count_if(registered.begin(), registered.end(), isZombie) == 1,
                "a pack override duplicated the vanilla spawn rule");
        const SpawnRule &rule = *std::find_if(registered.begin(), registered.end(), isZombie);
        require(rule.mConditions.size() == 1 && rule.mConditions[0].mOnSurface && rule.mConditions[0].mWeight == 12,
                "the nested pack spawn rule was not loaded");
        SpawnRules::initialize();
        const auto vanilla = std::find_if(registered.begin(), registered.end(), isZombie);
        require(vanilla != registered.end(), "the vanilla zombie spawn rule was not restored");
        require(vanilla->mConditions.size() != 1 || vanilla->mConditions[0].mWeight != 12,
                "a pack spawn rule survived registry initialization");

        std::filesystem::create_directories(pack / "items");
        std::ofstream(pack / "items" / "test.json") << R"({
            "format_version": "1.26.0",
            "minecraft:item": {"description": {"identifier": "example:catalog_item"}, "components": {}}
        })";
        std::filesystem::create_directories(pack / "item_catalog");
        std::ofstream(pack / "item_catalog" / "crafting_item_catalog.json") << R"({
            "format_version": "1.21.60",
            "minecraft:crafting_items_catalog": {
                "categories": [{"category_name": "construction", "groups": [{
                    "group_identifier": {"name": "example:building", "icon": "minecraft:stone"},
                    "items": ["example:catalog_item"]
                }]}]
            }
        })";
    }

    void writeJigsawPack(const std::filesystem::path &pack) {
        const auto worldgen = pack / "worldgen";
        for (const char *folder: {"processors", "template_pools", "jigsaw_structures", "structure_sets"})
            std::filesystem::create_directories(worldgen / folder);
        std::ofstream(worldgen / "template_pools" / "test.json") << R"({
            "minecraft:template_pool": {
                "description": {"identifier": "example:pool"},
                "fallback": "minecraft:empty",
                "elements": [{"weight": 1, "element": {
                    "element_type": "minecraft:single_pool_element", "projection": "rigid",
                    "location": "example:piece", "processors": "example:replace"
                }}]
            }
        })";
        std::ofstream(worldgen / "processors" / "replace.json") << R"({
            "minecraft:processor_list": {
                "description": {"identifier": "example:replace"},
                "processors": [{"processor_type": "minecraft:rule", "rules": [{
                    "input_predicate": {"predicate_type": "minecraft:block_match", "block": "minecraft:gold_block"},
                    "output_state": {"name": "minecraft:diamond_block"}
                }]}]
            }
        })";
        std::ofstream(worldgen / "jigsaw_structures" / "test.json") << R"({
            "minecraft:jigsaw": {
                "description": {"identifier": "example:test"},
                "start_pool": "example:pool",
                "max_depth": 0,
                "start_height": {"type": "uniform", "min": {"below_top": 20}, "max": {"absolute": 300}}
            }
        })";
        std::ofstream(worldgen / "structure_sets" / "test.json") << R"({
            "minecraft:structure_set": {
                "description": {"identifier": "example:test_set"},
                "placement": {"type": "minecraft:random_spread", "spacing": 8, "separation": 7, "salt": 123},
                "structures": [{"structure": "example:test", "weight": 1}]
            }
        })";
        std::ofstream(worldgen / "jigsaw_structures" / "padded.json") << R"({
            "minecraft:jigsaw": {
                "description": {"identifier": "example:padded"},
                "start_pool": "example:pool",
                "start_height": {"type": "constant", "value": {"absolute": 300}},
                "dimension_padding": {"top": 40, "bottom": 0}
            }
        })";
        std::ofstream(worldgen / "structure_sets" / "padded.json") << R"({
            "minecraft:structure_set": {
                "description": {"identifier": "example:padded_set"},
                "placement": {"type": "minecraft:random_spread", "spacing": 8, "separation": 7},
                "structures": [{"structure": "example:padded"}]
            }
        })";
        std::ofstream(worldgen / "structure_sets" / "invalid.json") << R"({
            "minecraft:structure_set": {
                "description": {"identifier": "example:invalid_set"},
                "placement": {"type": "minecraft:random_spread", "spacing": 8, "separation": 8},
                "structures": [{"structure": "example:test"}]
            }
        })";
        const auto structures = pack / "structures" / "example";
        std::filesystem::create_directories(structures);
        McStructure piece(Vector3i(1, 1, 1));
        const BlockState marker("minecraft:gold_block");
        piece.setBlock(Vector3i(0, 0, 0), &marker);
        std::ofstream(structures / "piece.mcstructure", std::ios::binary) << piece.serialize();
    }

    void writeWorldgenPack(const std::filesystem::path &pack) {
        for (const char *folder: {"biomes", "features", "feature_rules"})
            std::filesystem::create_directories(pack / folder);
        const std::string targets = [&] {
            std::string result;
            for (const std::string &name: BiomeChunkGenDataRegistry::getBiomeNames()) {
                if (!result.empty())
                    result += ',';
                result += json::stringify(*json::Value::ofString(name));
            }
            return result;
        }();
        std::ofstream(pack / "biomes" / "gem_fields.json") << R"({
            "format_version": "1.26.30",
            "minecraft:biome": {
                "description": {"identifier":"example:gem_fields"},
                "components": {
                    "minecraft:climate": {"temperature":0.6,"downfall":0},
                    "minecraft:tags": {"tags":["example_gem_fields"]},
                    "minecraft:surface_builder":{"builder":{
                        "type":"minecraft:overworld", "top_material":"minecraft:calcite",
                        "sea_floor_material":"minecraft:calcite", "mid_material":"minecraft:stone"
                    }},
                    "minecraft:subsurface_builder":{"builder":{
                        "type":"minecraft:noise_gradient",
                        "noise_descriptor":{"name":"example:gradient","first_octave":-5,"amplitudes":[1,0,1]},
                        "non_replaceable_blocks":["minecraft:bedrock"],
                        "noise_block_specifiers":[{"range":{"min":-1,"max":1},"block":"minecraft:smooth_basalt"}]
                    }},
                    "minecraft:replace_biomes": {"replacements":[{
                        "dimension":"minecraft:overworld","amount":1,"noise_frequency_scale":1,"targets":[)"
            << targets << R"(]}]}
                }
            }
        })";
        std::ofstream(pack / "biomes" / "ocean.json") << R"({
            "format_version":"1.26.30",
            "minecraft:biome":{
                "description":{"identifier":"minecraft:ocean"},
                "components":{"minecraft:tags":{"tags":["example_ocean_override"]}}
            }
        })";
        std::ofstream(pack / "features" / "gem_block.json") << R"({
            "format_version":"1.21.20",
            "minecraft:single_block_feature": {
                "description":{"identifier":"example:gem_block"},
                "places_block":"minecraft:emerald_block",
                "may_replace":["minecraft:air","minecraft:water"]
            }
        })";
        std::ofstream(pack / "features" / "gem_scatter.json") << R"({
            "format_version":"1.21.20",
            "minecraft:scatter_feature": {
                "description":{"identifier":"example:gem_scatter"},
                "places_feature":"example:gem_block",
                "distribution":{"iterations":1,"x":1,"y":2,"z":1}
            }
        })";
        std::ofstream(pack / "feature_rules" / "gems.json") << R"({
            "format_version":"1.21.20",
            "minecraft:feature_rules":{
                "description":{"identifier":"example:gems","places_feature":"example:gem_scatter"},
                "conditions":{"placement_pass":"final_pass","minecraft:biome_filter":{"all_of":[
                    {"test":"has_biome_tag","value":"example_gem_fields"},
                    {"none_of":[{"test":"has_biome_tag","value":"does_not_exist"}]}
                ]}},
                "distribution":{"iterations":"1 + 1","x":3,"y":270,"z":5}
            }
        })";
        std::ofstream(pack / "features" / "ore_group.json") << R"({
            "format_version":"1.21.20",
            "minecraft:ore_feature": {
                "description":{"identifier":"example:ore_group"}, "count":16,
                "replace_rules":[{"places_block":"minecraft:lapis_block","may_replace":["minecraft:air"]}]
            }
        })";
        std::ofstream(pack / "feature_rules" / "ores.json") << R"({
            "format_version":"1.21.20",
            "minecraft:feature_rules":{
                "description":{"identifier":"example:ores","places_feature":"example:ore_group"},
                "conditions":{"placement_pass":"after_surface_pass",
                              "minecraft:biome_filter":{"test":"has_biome_tag","value":"example_gem_fields"}},
                "distribution":{"iterations":1,"x":0,"y":200,"z":0}
            }
        })";
        std::ofstream(pack / "features" / "snap.json") << R"({
            "format_version":"1.21.20",
            "minecraft:snap_to_surface_feature": {
                "description":{"identifier":"example:snap"}, "feature_to_snap":"example:gem_block",
                "vertical_search_range":110, "surface":"floor", "allow_underwater_placement":true,
                "allowed_surface_blocks":["minecraft:calcite"]
            }
        })";
        std::ofstream(pack / "feature_rules" / "snapped.json") << R"({
            "format_version":"1.21.20",
            "minecraft:feature_rules":{
                "description":{"identifier":"example:snapped","places_feature":"example:snap"},
                "conditions":{"placement_pass":"after_surface_pass",
                              "minecraft:biome_filter":{"test":"has_biome_tag","value":"example_gem_fields"}},
                "distribution":{"iterations":1,"x":2,"y":120,"z":2}
            }
        })";
        std::ofstream(pack / "features" / "template.json") << R"({
            "format_version":"1.26.30",
            "minecraft:structure_template_feature":{
                "description":{"identifier":"example:template"},
                "structure_name":"example:piece","facing_direction":"north",
                "constraints":{"block_intersection":{"block_allowlist":["minecraft:air"]}}
            }
        })";
        std::ofstream(pack / "features" / "search.json") << R"({
            "format_version":"1.21.20",
            "minecraft:search_feature":{
                "description":{"identifier":"example:search"}, "places_feature":"example:gem_block",
                "search_axis":"+x","search_volume":{"min":[0,0,0],"max":[1,0,0]},"required_successes":2
            }
        })";
        const auto rule = [&](const char *name, const char *feature, const char *pass, int32_t x) {
            std::ofstream file(pack / "feature_rules" / (std::string(name) + ".json"));
            file << R"({"format_version":"1.21.20","minecraft:feature_rules":{
                "description":{"identifier":"example:)" << name << R"(","places_feature":"example:)" << feature << R"("},
                "conditions":{"placement_pass":")" << pass << R"(",
                    "minecraft:biome_filter":{"test":"has_biome_tag","value":"example_gem_fields"}},
                "distribution":{"iterations":1,"x":)" << x << R"(,"y":280,"z":8}}})";
        };
        rule("template", "template", "surface_pass", 12);
        rule("search_ok", "search", "final_pass", 2);
        rule("search_fail", "search", "final_pass", 11);
    }

    void writeAnimationPack(const std::filesystem::path &pack) {
        for (const char *folder: {"entities", "animations", "animation_controllers"})
            std::filesystem::create_directories(pack / folder);
        std::ofstream(pack / "entities" / "animated.json") << R"({
            "format_version":"1.21.20",
            "minecraft:entity": {
                "description":{
                    "identifier":"example:animated",
                    "is_summonable":true,
                    "properties":{"example:stage":{"type":"int","range":[0,4],"default":0}},
                    "animations":{"controller":"controller.animation.example","timeline":"animation.example"},
                    "scripts":{"initialize":["v.enter = 0;"],"animate":["controller"]}
                },
                "events":{
                    "example:entry":{"set_property":{"example:stage":1}},
                    "example:frame":{"set_property":{"example:stage":2}},
                    "example:exit":{"set_property":{"example:stage":3}},
                    "example:next":{"set_property":{"example:stage":4}}
                }
            }
        })";
        std::ofstream(pack / "animations" / "example.json") << R"({
            "format_version":"1.8.0",
            "animations":{
                "animation.example":{
                    "animation_length":0.15,
                    "timeline":{"0.1":["@s example:frame","v.finished = 1;"]}
                }
            }
        })";
        std::ofstream(pack / "animation_controllers" / "example.json") << R"json({
            "format_version":"1.10.0",
            "animation_controllers":{
                "controller.animation.example":{
                    "initial_state":"waiting",
                    "states":{
                        "waiting":{
                            "on_entry":["v.enter = 1;", "@s example:entry"],
                            "animations":[{"timeline":"v.enter == 1"}],
                            "transitions":[{"finished":"v.finished && q.all_animations_finished && (math.sin(30) > 0.49 ? math.hermite_blend(0.5) == 0.5 : 0)"}],
                            "on_exit":["@s example:exit"]
                        },
                        "finished":{"on_entry":["@s example:next"]}
                    }
                }
            }
        })json";
    }

    void checkServerLoadsPackData(const std::filesystem::path &directory) {
        const auto pack = directory / "behavior_packs" / "spawn-override";
        writeJigsawPack(pack);
        writeWorldgenPack(pack);
        writeAnimationPack(pack);
        ServerPaths::setRoot(directory);
        ServerHostOptions options;
        options.bindAddress = "127.0.0.1";
        options.portOverride = 0;
        options.plugins = false;
        options.properties = {{"level-name", "PackTest"}, {"level-seed", "1"}, {"max-threads", "1"},
                              {"online-mode", "false"}, {"auto-compaction-interval", "0"}};
        ServerHost host;
        require(host.start(options), "could not start server for the catalog test");
        auto &handler = host.getHandler();
        const auto &items = handler.getCreativeItems();
        const auto isCatalogItem = [](const CreativeItemData &entry) {
            return entry.mItem.mDefinition != nullptr
                   && entry.mItem.mDefinition->getIdentifier() == "example:catalog_item";
        };
        const auto item = std::find_if(items.begin(), items.end(), isCatalogItem);
        require(item != items.end() && std::count_if(items.begin(), items.end(), isCatalogItem) == 1,
                "the catalog item was missing or duplicated");
        const auto &groups = handler.getCreativeGroups();
        require(item->mGroupIndex >= 0 && (size_t) item->mGroupIndex < groups.size(),
                "the catalog item did not get a valid group");
        const auto &group = groups[(size_t) item->mGroupIndex];
        require(group.mName == "example:building" && group.mCategory == 1 && group.mIcon.mDefinition != nullptr
                        && group.mIcon.mDefinition->getIdentifier() == "minecraft:stone",
                "the catalog did not override the item's group, category and icon");
        require(CameraPresetRegistry::runtimeId("example:child") >= 0, "server startup omitted pack cameras");
        const auto greeting = Localization::getInstance().translate("fr_FR", "pack.test.greeting", {"Alex"});
        require(greeting == "Bonjour Alex", "server startup omitted pack texts");
        const auto &jigsaws = JigsawStructures::packStructures();
        require(jigsaws.sets().size() == 2, "server startup loaded an invalid set or omitted a valid pack structure set");
        const auto generator = DimensionFactory::createGenerator(DimensionType::Overworld, 1);
        const auto findSet = [&jigsaws](const std::string &id) -> const JigsawStructures::Set & {
            const auto found = std::find_if(jigsaws.sets().begin(), jigsaws.sets().end(),
                                            [&id](const auto &set) { return set->mIdentifier == id; });
            require(found != jigsaws.sets().end(), "missing pack structure set");
            return **found;
        };
        require(jigsaws.plan(*generator, findSet("example:padded_set"), 0, 0) == nullptr,
                "the structure ignored its top padding");
        const auto plan = jigsaws.plan(*generator, findSet("example:test_set"), 0, 0);
        require(plan != nullptr, "the registered set could not resolve its pack template");
        require(plan->mPieces.front().y >= LevelChunk::MAX_Y - 21 && plan->mPieces.front().y <= 299,
                "the start height did not use the pack's uniform anchors");
        TerrainCache terrain(*generator);
        GeneratorChunkSource source(1, DimensionType::Overworld, terrain);
        std::vector<Tag> blockActors;
        std::vector<Tag> entities;
        const LevelChunk chunk = source.generate(0, 0, blockActors, entities);
        const auto &piece = plan->mPieces.front();
        require(chunk.getBlock(piece.x & 15, piece.y, piece.z & 15).mName == "minecraft:diamond_block",
                "a newly generated chunk omitted the pack structure or its block processor");
        const int32_t ocean = BiomeChunkGenDataRegistry::getBiomeId("minecraft:ocean");
        require(ocean >= 0 && BiomeChunkGenDataRegistry::hasTag(ocean, "example_ocean_override")
                && !BiomeChunkGenDataRegistry::hasTag(ocean, "overworld"),
                "a vanilla biome override did not replace its tags");
        const int32_t customBiome = BiomeChunkGenDataRegistry::getBiomeId("example:gem_fields");
        require(customBiome >= 0 && (int32_t) chunk.getBiomeAt(3, 270, 5) == customBiome,
                "the generator did not apply the custom biome replacement");
        require(chunk.getBlock(4, 272, 6).mName == "minecraft:emerald_block",
                "the biome-filtered feature or nested scatter was not applied to the new chunk");
        bool hasCalciteSurface = false;
        for (int32_t y = 40; y < 110; ++y)
            hasCalciteSurface |= chunk.getBlock(2, y, 2).mName == "minecraft:calcite";
        require(hasCalciteSurface, "the custom biome did not apply its surface materials");
        bool snappedToCalcite = false;
        for (int32_t y = 40; y < 110; ++y) {
            if (chunk.getBlock(2, y, 2).mName == "minecraft:calcite"
                && chunk.getBlock(2, y + 1, 2).mName == "minecraft:emerald_block")
                snappedToCalcite = true;
        }
        require(snappedToCalcite, "the feature did not snap to the biome surface");
        bool hasOreGroup = false;
        for (int32_t x = 3; x < 13; ++x) {
            for (int32_t z = 3; z < 13; ++z) {
                for (int32_t y = 196; y < 202; ++y)
                    hasOreGroup |= chunk.getBlock(x, y, z).mName == "minecraft:lapis_block";
            }
        }
        require(hasOreGroup, "the pack-defined ore vein was not placed");
        bool hasGradient = false;
        for (int32_t y = -56; y < -24; ++y)
            hasGradient |= chunk.getBlock(2, y, 2).mName == "minecraft:smooth_basalt";
        require(hasGradient && chunk.getBlock(2, LevelChunk::MIN_Y, 2).mName == "minecraft:bedrock",
                "the subsurface noise gradient did not place its material or preserve bedrock");
        require(chunk.getBlock(12, 280, 8).mName == "minecraft:gold_block",
                "the structure template feature did not place the template");
        require(chunk.getBlock(2, 280, 8).mName == "minecraft:emerald_block"
                && chunk.getBlock(3, 280, 8).mName == "minecraft:emerald_block",
                "the search feature did not place both successful results");
        require(chunk.getBlock(11, 280, 8).mName == "minecraft:air",
                "a search feature below its success threshold left a block behind");
        const auto &biomes = handler.getBiomes().getBiomes();
        require(std::any_of(biomes.begin(), biomes.end(), [customBiome](const auto &definition) {
                    return definition.mId == customBiome && definition.mName == "example:gem_fields"
                           && !definition.mRain && definition.mTemperature > 0.59f;
                }), "the custom biome was missing from the client registry");

        ServerActor *actor = handler.spawnActor(handler.getLevel(), "example:animated", Vector3f(0, 90, 0));
        require(actor != nullptr, "could not spawn actor for the animation test");
        handler.tickActors();
        require(actor->getIntProperty("example:stage", 0) == 1, "the controller did not enter its initial state");
        handler.tickActors();
        require(actor->getIntProperty("example:stage", 0) == 2, "the animation timeline did not trigger its frame event");
        handler.tickActors();
        require(actor->getIntProperty("example:stage", 0) == 4, "the controller did not transition after the animation");
        host.stop();
        require(BiomeChunkGenDataRegistry::getBiomeId("example:gem_fields") < 0
                && std::filesystem::is_regular_file(directory / "custom-biome-ids.json"),
                "the custom biome registry was not cleared or its ID was not persisted");
        require(jigsaws.sets().empty(), "pack structures remained loaded after the server stopped");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-pack-data-" + std::to_string(
                                      std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        SpawnRules::initialize();
        BiomeChunkGenDataRegistry::initialize();
        checkPackSpawnRules(directory);
        checkServerLoadsPackData(directory);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    return result;
}
