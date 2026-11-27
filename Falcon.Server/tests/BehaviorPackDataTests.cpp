#include "Network/ServerNetworkHandler.h"
#include "Scripting/BehaviorPackManager.h"
#include "Server/CameraPresetRegistry.h"
#include "Server/Localization.h"
#include "Server/ServerHost.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityEvents.h"
#include "World/Actor/Spawn/NaturalSpawner.h"
#include "World/Actor/Spawn/SpawnRules.h"
#include "World/Level/LevelGen/DimensionFactory.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/GeneratorChunkSource.h"
#include "World/Level/LevelGen/Structure/JigsawStructures.h"
#include "World/Level/Storage/FalconDataVersion.h"
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
        std::ofstream(pack / "biomes" / "the_end.json") << R"({
            "format_version":"1.26.30",
            "minecraft:biome":{
                "description":{"identifier":"minecraft:the_end"},
                "components":{"minecraft:tags":{"tags":["example_end"]}}
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
        std::ofstream(pack / "feature_rules" / "end_gem.json") << R"({
            "format_version":"1.21.20",
            "minecraft:feature_rules":{
                "description":{"identifier":"example:end_gem","places_feature":"example:gem_block"},
                "conditions":{"placement_pass":"final_pass",
                    "minecraft:biome_filter":{"test":"has_biome_tag","value":"example_end"}},
                "distribution":{"iterations":1,"x":4,"y":240,"z":4}
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
        McStructure asymmetric(Vector3i(2, 1, 1));
        const BlockState emerald("minecraft:emerald_block");
        asymmetric.setBlock(Vector3i(1, 0, 0), &emerald);
        std::ofstream(pack / "structures" / "example" / "asymmetric.mcstructure", std::ios::binary)
                << asymmetric.serialize();
        std::ofstream(pack / "features" / "rotated.json") << R"({
            "format_version":"1.26.30",
            "minecraft:structure_template_feature":{
                "description":{"identifier":"example:rotated"},
                "structure_name":"example:asymmetric","facing_direction":"north"
            }
        })";
        McStructure sparse(Vector3i(2, 1, 2));
        sparse.setBlock(Vector3i(0, 0, 0), &emerald);
        sparse.setBlock(Vector3i(1, 0, 1), &emerald);
        std::ofstream(pack / "structures" / "example" / "sparse.mcstructure", std::ios::binary)
                << sparse.serialize();
        std::ofstream(pack / "features" / "level_sparse.json") << R"({
            "format_version":"1.26.30",
            "minecraft:structure_template_feature":{
                "description":{"identifier":"example:level_sparse"},
                "structure_name":"example:sparse","facing_direction":"south",
                "constraints":{"leveled":{"max_steepness":0}}
            }
        })";
        McStructure raised(Vector3i(1, 2, 1));
        raised.setBlock(Vector3i(0, 0, 0), &emerald);
        raised.setBlock(Vector3i(0, 1, 0), &emerald);
        std::ofstream(pack / "structures" / "example" / "raised.mcstructure", std::ios::binary)
                << raised.serialize();
        std::ofstream(pack / "features" / "level_raised.json") << R"({
            "format_version":"1.26.30",
            "minecraft:structure_template_feature":{
                "description":{"identifier":"example:level_raised"},
                "structure_name":"example:raised","facing_direction":"south","ground_level":1,
                "constraints":{"leveled":{"max_steepness":0}}
            }
        })";
        std::ofstream(pack / "features" / "adjusted.json") << R"({
            "format_version":"1.26.30",
            "minecraft:structure_template_feature":{
                "description":{"identifier":"example:adjusted"},
                "structure_name":"example:piece","facing_direction":"north","adjustment_radius":1,
                "constraints":{"block_intersection":{"block_allowlist":["minecraft:air"]}}
            }
        })";
        std::ofstream(pack / "features" / "level.json") << R"({
            "format_version":"1.26.30",
            "minecraft:structure_template_feature":{
                "description":{"identifier":"example:level"},
                "structure_name":"example:piece","facing_direction":"north",
                "constraints":{"leveled":{"max_steepness":0}}
            }
        })";
        std::ofstream(pack / "features" / "level_default.json") << R"({
            "format_version":"1.26.30",
            "minecraft:structure_template_feature":{
                "description":{"identifier":"example:level_default"},
                "structure_name":"example:piece","facing_direction":"north",
                "constraints":{"leveled":{}}
            }
        })";
        McStructure wide(Vector3i(34, 1, 1));
        wide.setBlock(Vector3i(0, 0, 0), &emerald);
        wide.setBlock(Vector3i(33, 0, 0), &emerald);
        std::ofstream(pack / "structures" / "example" / "wide.mcstructure", std::ios::binary)
                << wide.serialize();
        std::ofstream(pack / "features" / "wide.json") << R"({
            "format_version":"1.26.30",
            "minecraft:structure_template_feature":{
                "description":{"identifier":"example:wide"},
                "structure_name":"example:wide","facing_direction":"south"
            }
        })";
        const auto rule = [&](const char *name, const char *feature, const char *pass, int32_t x, int32_t y = 280,
                              int32_t z = 8) {
            std::ofstream file(pack / "feature_rules" / (std::string(name) + ".json"));
            file << R"({"format_version":"1.21.20","minecraft:feature_rules":{
                "description":{"identifier":"example:)" << name << R"(","places_feature":"example:)" << feature << R"("},
                "conditions":{"placement_pass":")" << pass << R"(",
                    "minecraft:biome_filter":{"test":"has_biome_tag","value":"example_gem_fields"}},
                "distribution":{"iterations":1,"x":)" << x << R"(,"y":)" << y
                     << R"(,"z":)" << z << "}}}";
        };
        rule("template", "template", "surface_pass", 12);
        rule("search_ok", "search", "final_pass", 2);
        rule("search_fail", "search", "final_pass", 11);
        rule("adjusted", "adjusted", "final_pass", 12);
        rule("rotated", "rotated", "final_pass", 9, 285);
        rule("pad", "template", "surface_pass", 14, 269, 9);
        rule("pad_lower", "template", "surface_pass", 14, 268, 10);
        rule("pad_too_low", "template", "surface_pass", 14, 268, 11);
        rule("leveled", "level", "final_pass", 14, 270, 9);
        rule("leveled_tolerance", "level_default", "final_pass", 14, 270, 10);
        rule("leveled_reject", "level", "final_pass", 14, 270, 11);
        rule("sparse_pad_1", "template", "surface_pass", 8, 274, 12);
        rule("sparse_pad_2", "template", "surface_pass", 9, 274, 13);
        rule("sparse_pad_3", "template", "surface_pass", 11, 274, 12);
        rule("sparse_ok", "level_sparse", "final_pass", 8, 275, 12);
        rule("sparse_reject", "level_sparse", "final_pass", 11, 275, 12);
        rule("raised_pad", "template", "surface_pass", 14, 286, 10);
        rule("raised_level", "level_raised", "final_pass", 14, 287, 10);
        rule("wide", "wide", "final_pass", 15, 290, 5);
    }

    void writeAnimationPack(const std::filesystem::path &pack) {
        for (const char *folder: {"entities", "animations", "animation_controllers"})
            std::filesystem::create_directories(pack / folder);
        const auto writeEntity = [&](const char *name, const char *components) {
            std::ofstream(pack / "entities" / (std::string(name) + ".json")) << R"({
            "format_version":"1.21.20",
            "minecraft:entity": {
                "description":{
                    "identifier":"example:)" << name << R"(",
                    "is_summonable":true,
                    "properties":{"example:stage":{"type":"int","range":[0,4],"default":0}},
                    "animations":{"controller":"controller.animation.example","timeline":"animation.example"},
                    "scripts":{"initialize":["v.enter = 0;"],"animate":["controller"]}
                },
                "components":)" << components << R"(,
                "events":{
                    "example:entry":{"set_property":{"example:stage":1}},
                    "example:frame":{"set_property":{"example:stage":2}},
                    "example:exit":{"set_property":{"example:stage":3}},
                    "example:next":{"set_property":{"example:stage":4}}
                }
            }
        })";
        };
        writeEntity("animated", "{}");
        writeEntity("animated_projectile", R"({"minecraft:projectile":{}})");
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

    void writeNaturalSpawnPack(const std::filesystem::path &pack) {
        std::ofstream(pack / "entities" / "natural.json") << R"({
            "format_version":"1.21.20",
            "minecraft:entity":{
                "description":{
                    "identifier":"example:natural",
                    "properties":{"example:stage":{"type":"int","range":[0,2],"default":0}}
                },
                "component_groups":{
                    "example:large":{"minecraft:collision_box":{"width":1.5,"height":2.5}}
                },
                "events":{
                    "minecraft:entity_spawned":{
                        "add":{"component_groups":["example:large"]},
                        "set_property":{"example:stage":1}
                    },
                    "example:reset":{
                        "remove":{"component_groups":["example:large"]},
                        "set_property":{"example:stage":2}
                    }
                }
            }
        })";
        std::ofstream(pack / "spawn_rules" / "natural.json") << R"({
            "format_version":"1.8.0",
            "minecraft:spawn_rules":{
                "description":{"identifier":"example:natural","population_control":"animal"},
                "conditions":[{
                    "minecraft:spawns_on_surface":{},
                    "minecraft:height_filter":{"min":251,"max":251},
                    "minecraft:weight":{"default":1000000000}
                }]
            }
        })";
    }

    void checkNaturalSpawn(ServerNetworkHandler &handler) {
        Level &level = handler.getLevel();
        const int32_t chunkCoordinate = 13;
        const BlockState floor("minecraft:stone");
        for (int32_t x = chunkCoordinate - 1; x <= chunkCoordinate + 1; ++x) {
            for (int32_t z = chunkCoordinate - 1; z <= chunkCoordinate + 1; ++z) {
                auto &chunk = level.insertChunk(LevelChunk(x, z));
                chunk.setPopulated(true);
                for (int32_t localX = 0; localX < 16; ++localX) {
                    for (int32_t localZ = 0; localZ < 16; ++localZ)
                        chunk.setBlock(localX, 250, localZ, floor);
                }
            }
        }
        level.setActiveColumns({BlockTickingQueue::column(chunkCoordinate, chunkCoordinate)});
        const uint64_t runtimeId = handler.allocateRuntimeId();
        const NetworkIdentifier id("pack-test-player", runtimeId);
        ServerPlayer &player = handler.getPlayers().try_emplace(id, id, runtimeId).first->second;
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setPosition(Vector3f(chunkCoordinate * 16 - 32, 251, chunkCoordinate * 16));
        player.setLoginState(ServerPlayer::LoginState::Spawned);

        NaturalSpawner spawner;
        ServerActor *natural = nullptr;
        for (int32_t tick = 0; tick < 8192 && natural == nullptr; ++tick) {
            spawner.tick(handler, level);
            for (const auto &[uniqueId, actor]: handler.getActors()) {
                if (actor->getTypeId() == "example:natural") {
                    natural = actor.get();
                    break;
                }
            }
        }
        handler.getPlayers().erase(id);
        level.setActiveColumns({});
        require(natural != nullptr, "the pack rule did not spawn its entity");
        handler.tickActors();
        require(natural->getIntProperty("example:stage") == 1 && natural->getSize().mHeight == 2.5f,
                "the default spawn event did not initialize the naturally spawned entity");
        EntityEvents::fire(handler, *natural, "example:reset");
        require(natural->getIntProperty("example:stage") == 2 && natural->getSize().mHeight == ServerActor::DEFAULT_HEIGHT,
                "removing the component group did not restore the base collision box");

        const int64_t uniqueId = natural->getUniqueId();
        const Tag legacyActor = natural->ServerActor::saveNbt();
        for (const Tag &savedActor: {natural->saveNbt(), legacyActor}) {
            handler.removeActor(uniqueId);
            handler.spawnStoredActors(level, {savedActor});
            const auto restored = handler.getActors().find(uniqueId);
            require(restored != handler.getActors().end(), "the naturally spawned entity did not reload");
            handler.tickActors();
            require(restored->second->getIntProperty("example:stage") == 2
                    && restored->second->getSize().mHeight == ServerActor::DEFAULT_HEIGHT,
                    "loading the entity replayed its default spawn event");
        }

        // generated actors have no saved identity and still need their initial event
        Tag generatedActor = legacyActor;
        generatedActor.remove("UniqueID");
        generatedActor.remove(FalconDataVersion::TAG);
        handler.removeActor(uniqueId);
        handler.spawnStoredActors(level, {generatedActor});
        handler.tickActors();
        const auto &actors = handler.getActors();
        require(std::any_of(actors.begin(), actors.end(), [](const auto &entry) {
                    const ServerActor &actor = *entry.second;
                    return actor.getTypeId() == "example:natural" && actor.getIntProperty("example:stage") == 1
                           && actor.getSize().mHeight == 2.5f;
                }), "the generated entity missed its first spawn event");
    }

    void checkServerLoadsPackData(const std::filesystem::path &directory) {
        const auto pack = directory / "behavior_packs" / "spawn-override";
        writeJigsawPack(pack);
        writeWorldgenPack(pack);
        writeAnimationPack(pack);
        writeNaturalSpawnPack(pack);
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
        require(chunk.getBlock(8, 285, 8).mName == "minecraft:emerald_block"
                && chunk.getBlock(10, 285, 8).mName == "minecraft:air",
                "a noncentered structure did not rotate around its origin");
        require(chunk.getBlock(13, 280, 8).mName == "minecraft:gold_block"
                && chunk.getBlock(12, 281, 8).mName == "minecraft:air",
                "the adjustment did not move east at the original height");
        require(chunk.getBlock(14, 270, 9).mName == "minecraft:gold_block"
                && chunk.getBlock(14, 270, 10).mName == "minecraft:gold_block"
                && chunk.getBlock(14, 270, 11).mName == "minecraft:air",
                "leveled did not respect zero and default steepness");
        require(chunk.getBlock(8, 275, 12).mName == "minecraft:emerald_block"
                && chunk.getBlock(9, 275, 13).mName == "minecraft:emerald_block"
                && chunk.getBlock(11, 275, 12).mName == "minecraft:air",
                "leveled did not check each occupied column of the ground layer");
        require(chunk.getBlock(14, 286, 10).mName == "minecraft:emerald_block"
                && chunk.getBlock(14, 287, 10).mName == "minecraft:emerald_block"
                && chunk.getBlock(14, 288, 10).mName == "minecraft:air",
                "the leveled check or placement ignored ground_level");
        require(chunk.getBlock(15, 290, 5).mName == "minecraft:air",
                "a template extending outside the staged chunks was partially placed");
        const auto endGenerator = DimensionFactory::createGenerator(DimensionType::TheEnd, 1);
        TerrainCache endTerrain(*endGenerator);
        GeneratorChunkSource endSource(1, DimensionType::TheEnd, endTerrain);
        const LevelChunk endChunk = endSource.generate(0, 0, blockActors, entities);
        require(endChunk.getBlock(4, 240, 4).mName == "minecraft:emerald_block",
                "the End did not place its biome-filtered pack feature");
        const auto &biomes = handler.getBiomes().getBiomes();
        require(std::any_of(biomes.begin(), biomes.end(), [customBiome](const auto &definition) {
                    return definition.mId == customBiome && definition.mName == "example:gem_fields"
                           && !definition.mRain && definition.mTemperature > 0.59f;
                }), "the custom biome was missing from the client registry");

        ServerActor *actor = handler.spawnActor(handler.getLevel(), "example:animated", Vector3f(0, 90, 0));
        ServerActor *projectile = handler.spawnActor(handler.getLevel(), "example:animated_projectile",
                                                      Vector3f(6, 180, 6));
        ServerActor *spawned = handler.spawnActor(handler.getLevel(), "example:animated", Vector3f(2, 90, 2),
                                                   {}, "example:next");
        require(actor != nullptr && projectile != nullptr && projectile->isProjectile() && spawned != nullptr,
                "could not spawn actors for the pack event tests");
        require(spawned->getIntProperty("example:stage", 0) == 4,
                "the pack actor missed its explicit spawn event");
        const uint64_t projectileId = projectile->getRuntimeId();
        const auto projectileStage = [&] {
            const auto *active = handler.getActorByRuntimeId(projectileId);
            require(active != nullptr, "the test projectile expired");
            return active->getIntProperty("example:stage", 0);
        };
        handler.tickActors();
        require(actor->getIntProperty("example:stage", 0) == 1, "the controller did not enter its initial state");
        require(projectileStage() == 1, "the projectile controller did not enter its initial state");
        handler.tickActors();
        require(actor->getIntProperty("example:stage", 0) == 2, "the animation timeline did not trigger its frame event");
        require(projectileStage() == 2, "the projectile animation timeline did not trigger its frame event");
        handler.tickActors();
        require(actor->getIntProperty("example:stage", 0) == 4, "the controller did not transition after the animation");
        require(projectileStage() == 4, "the projectile controller did not transition after the animation");
        checkNaturalSpawn(handler);
        host.stop();
        require(BiomeChunkGenDataRegistry::getBiomeId("example:gem_fields") < 0
                && std::filesystem::is_regular_file(directory / "custom-biome-ids.json"),
                "the custom biome registry was not cleared or its ID was not persisted");
        require(jigsaws.sets().empty(), "pack structures remained loaded after the server stopped");

        std::filesystem::rename(pack, directory / "disabled-pack");
        require(host.start(options), "could not restart the server with the pack disabled");
        const auto &registry = host.getHandler().getBiomes();
        const auto &retiredBiomes = registry.getBiomes();
        const auto &strings = registry.getStrings();
        require(std::any_of(retiredBiomes.begin(), retiredBiomes.end(), [&](const auto &definition) {
                    return definition.mId == customBiome && definition.mName == "example:gem_fields"
                           && definition.mNameIndex < strings.size()
                           && strings[definition.mNameIndex] == definition.mName;
                }), "the disabled pack biome was missing from the client registry");
        require(BiomeChunkGenDataRegistry::getBiomeId("example:gem_fields") < 0,
                "the disabled pack biome was still available to the generator");
        host.stop();
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
