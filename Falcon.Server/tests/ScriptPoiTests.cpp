#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "Server/ServerPaths.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"
#include "World/Village/PoiBlockTypes.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void writeFile(const std::filesystem::path &path, const std::string &contents) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << contents;
    }

    void writePack(const std::filesystem::path &root, const std::string &name, const std::string &script) {
        writeFile(root / name / "manifest.json",
                  "{\"format_version\": 2, \"header\": {\"name\": \"" + name
                          + "\", \"uuid\": \"00000000-0000-4000-8000-400000000081\", \"version\": [1, 0, 0],"
                            " \"min_engine_version\": [1, 21, 0]},"
                            " \"modules\": [{\"type\": \"script\", \"language\": \"javascript\","
                            " \"uuid\": \"00000000-0000-4000-9000-400000000081\", \"entry\": \"scripts/main.js\","
                            " \"version\": [1, 0, 0]}],"
                            " \"dependencies\": [{\"module_name\": \"@minecraft/server\", \"version\": \"beta\"}]}\n");
        writeFile(root / name / "scripts" / "main.js", script);
    }

    const char *const POI = R"JS(
import * as server from "@minecraft/server";
const { world, system } = server;

const r = {};
const save = () => world.setDynamicProperty("poi", JSON.stringify(r));
const fail = (f) => { try { f(); return "ok"; } catch (e) { return e.name + ":" + e.message; } };
const v = (p) => p === undefined ? "none" : p.x + "," + p.y + "," + p.z;
const all = () => true;

system.afterEvents.scriptEventReceive.subscribe((event) => {
    const dimension = world.getDimension("overworld");
    if (event.id === "poi:gated") {
        r.gated = [typeof server.PoiManager, typeof server.PoiBlockManager, typeof server.PoiBlockType,
            typeof server.PoiBlockInstance, String(dimension.poiManager)].join("|");
        save();
        return;
    }
    const blocks = dimension.poiManager.blocks;
    const composter = { x: 1, y: 70, z: 1 };
    if (event.id === "poi:query") {
        r.classes = [dimension.poiManager instanceof server.PoiManager, blocks instanceof server.PoiBlockManager]
            .join("|");
        const farmer = blocks.at(composter);
        r.type = [farmer instanceof server.PoiBlockType, farmer.id, farmer.name, farmer.tickets, farmer.usableRange,
            farmer.hasTag("minecraft:acquirable_job_site"), farmer.hasTag("minecraft:village"),
            farmer.hasTag("minecraft:bee_home")].join("|");
        const meeting = blocks.at({ x: 3.7, y: 70.9, z: 1.2 });
        r.meeting = [meeting.id, meeting.name, meeting.tickets, meeting.usableRange,
            meeting.hasTag("minecraft:village"), meeting.hasTag("minecraft:acquirable_job_site")].join("|");
        r.none = String(blocks.at({ x: 0, y: 70, z: 0 }));
        r.equals = [farmer.equals(blocks.at(composter)), farmer.equals(meeting)].join("|");
        r.exists = [blocks.exists(composter, { name: "minecraft:farmer" }),
            blocks.exists(composter, { tags: ["minecraft:village", "minecraft:acquirable_job_site"] }),
            blocks.exists(composter, { tags: ["minecraft:bee_home"] }),
            blocks.exists(composter, (t) => t.name === "minecraft:meeting"),
            blocks.exists({ x: 0, y: 70, z: 0 }, all)].join("|");
        r.square = blocks.getInSquare({ tags: ["minecraft:village"] }, { x: 1.5, y: 70.2, z: 1.5 }, 2)
            .map((i) => i.type.name + "@" + v(i.position) + "#" + i.tickets).sort().join(";");
        r.range = blocks.getInRange(all, composter, 2).map((i) => i.type.name).sort().join(";");
        r.sorted = blocks.getInRangeSorted(all, composter, 3)
            .map((p) => p.distance + ":" + p.poi.type.name).join(";");
        r.take = [v(blocks.take({ name: "minecraft:farmer" }, composter, 1)),
            v(blocks.take({ name: "minecraft:farmer" }, composter, 1)),
            blocks.getInRange({ name: "minecraft:farmer" }, composter, 1, server.PoiBlockOccupancyFilter.Full).length,
            blocks.getInRange({ name: "minecraft:farmer" }, composter, 1, "HasVacancy").length,
            blocks.release(composter), blocks.release(composter), blocks.release({ x: 3, y: 70, z: 3 }),
            blocks.release({ x: 0, y: 70, z: 0 })].join("|");
        r.takeNone = v(blocks.take({ name: "minecraft:lodestone" }, composter, 5));
        blocks.addTemporary({ x: 5, y: 70, z: 5 }, "minecraft:home");
        blocks.addTemporary({ x: 6, y: 70, z: 5 }, 4);
        blocks.addTemporary({ x: 7, y: 70, z: 5 }, meeting);
        blocks.addTemporary({ x: 8, y: 70, z: 5 }, "minecraft:nothing");
        r.temporary = [blocks.at({ x: 5, y: 70, z: 5 }).name, blocks.at({ x: 6, y: 70, z: 5 }).name,
            blocks.at({ x: 7, y: 70, z: 5 }).name, String(blocks.at({ x: 8, y: 70, z: 5 })),
            blocks.getInSquare(all, { x: 6, y: 70, z: 5 }, 2).length].join("|");
        r.thrown = [fail(() => blocks.getInRange(() => { throw new Error("boom"); }, composter, 1)),
            fail(() => blocks.at("nope")), fail(() => blocks.getInRange(all, composter, 1, "Half")),
            fail(() => blocks.exists(composter, 5)), fail(() => farmer.equals({}))].join("|");
    } else if (event.id === "poi:after") {
        r.after = [String(blocks.at(composter)), v(blocks.take(all, { x: 5, y: 70, z: 5 }, 0)),
            blocks.at({ x: 5, y: 70, z: 5 }).name].join("|");
    }
    save();
});
)JS";

    std::unique_ptr<json::Value> result(ServerNetworkHandler &owner) {
        const auto it = owner.getWorldDynamicProperties().find("poi");
        require(it != owner.getWorldDynamicProperties().end(), "no result from the poi pack");
        std::unique_ptr<json::Value> parsed = json::parse(it->second.mString);
        require(parsed != nullptr && parsed->isObject(), "the poi pack wrote no JSON");
        return parsed;
    }

    void expect(const json::Value &values, const char *key, const std::string &wanted) {
        const json::Value *value = values.get(key);
        require(value != nullptr, std::string("missing result ") + key);
        const std::string actual = value->mType == json::Value::Type::Boolean ? (value->mBoolean ? "true" : "false")
                                                                              : value->string();
        require(actual == wanted, std::string(key) + ": expected \"" + wanted + "\", got \"" + actual + "\"");
    }

    BlockState state(const char *identifier) {
        const Block *block = VanillaBlocks::fromIdentifier(identifier);
        require(block != nullptr, std::string("no block ") + identifier);
        return block->toBlockState();
    }

    void checkTypes() {
        const std::vector<PoiBlockType> &types = PoiBlockTypes::getAll();
        require(types.size() == 19, "the experiment has nineteen poi types");
        require(types[0].mName == "minecraft:armorer" && types[18].mName == "minecraft:weaponsmith",
                "the poi types are out of the archive's order");
        const PoiBlockType *rod = PoiBlockTypes::forBlock("minecraft:waxed_weathered_lightning_rod");
        require(rod != nullptr && rod->mName == "minecraft:lightning_rod" && rod->mTags.empty(),
                "a waxed lightning rod is no lightning_rod poi");
        require(PoiBlockTypes::forBlock("minecraft:lit_blast_furnace") == nullptr,
                "a lit blast furnace became a poi the documents leave out");
    }

    void run(const std::filesystem::path &directory, bool experiment) {
        const std::filesystem::path packs = directory / (experiment ? "packs-on" : "packs-off");
        writePack(packs, "poi", POI);

        ServerNetworkHandler owner("Script poi", "", 1);
        if (experiment)
            owner.getLevel().setExperiments({"epoi"});
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);
        BehaviorPackManager manager;
        manager.discover(packs.string());
        CustomContentRegistry::getInstance().load(manager, owner.getItemDefinitions(), owner.getBlockDefinitions());
        require(engine.loadPacks(manager.getPacks()) == 1, "the poi pack did not start");
        engine.startup();

        World &world = owner.getWorlds().getDefault();
        Level &level = world.getOverworld();
        LevelChunk chunk(0, 0);
        chunk.setBlock(1, 70, 1, state("minecraft:composter"));
        chunk.setBlock(3, 70, 1, state("minecraft:bell"));
        chunk.setBlock(3, 70, 3, state("minecraft:lodestone"));
        level.insertChunk(std::move(chunk));
        engine.tick(0);

        if (!experiment) {
            engine.emitScriptEvent("poi:gated", "");
            expect(*result(owner), "gated", "undefined|undefined|undefined|undefined|undefined");
            engine.shutdown();
            return;
        }

        engine.emitScriptEvent("poi:query", "");
        std::unique_ptr<json::Value> r = result(owner);
        expect(*r, "classes", "true|true");
        expect(*r, "type", "true|6|minecraft:farmer|1|1|true|true|false");
        expect(*r, "meeting", "15|minecraft:meeting|32|6|true|false");
        expect(*r, "none", "undefined");
        expect(*r, "equals", "true|false");
        expect(*r, "exists", "true|true|false|false|false");
        expect(*r, "square", "minecraft:farmer@1,70,1#1;minecraft:meeting@3,70,1#32");
        expect(*r, "range", "minecraft:farmer;minecraft:meeting");
        expect(*r, "sorted", "0:minecraft:farmer;4:minecraft:meeting;8:minecraft:lodestone");
        expect(*r, "take", "1,70,1|none|1|0|true|false|false|false");
        expect(*r, "takeNone", "none");
        expect(*r, "temporary", "minecraft:home|minecraft:cartographer|minecraft:meeting|undefined|3");
        expect(*r, "thrown",
               "Error:boom|TypeError:Native type conversion failed.|TypeError:Native type conversion failed.|"
               "TypeError:Native type conversion failed.|TypeError:Native type conversion failed.");

        // a broken block takes its poi along; the temporary one stays while its chunk does
        level.setBlock(Vector3i(1, 70, 1), state("minecraft:air"), true);
        engine.emitScriptEvent("poi:after", "");
        r = result(owner);
        expect(*r, "after", "undefined|5,70,5|minecraft:home");
        engine.shutdown();
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-script-poi-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        checkTypes();
        run(directory, false);
        CustomContentRegistry::getInstance().reset();
        run(directory, true);
    } catch (const std::exception &error) {
        std::clog << "Script poi regression failed: " << error.what() << '\n';
        result = 1;
    }
    CustomContentRegistry::getInstance().reset();
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
