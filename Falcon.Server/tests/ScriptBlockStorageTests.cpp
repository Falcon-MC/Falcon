#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "Server/ServerPaths.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/BlockEntityStorage.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// tick() returns at once while the server does not listen: the main thread task step of a tick, where a stored
// entity leaves the world
class ServerTickProbe {
public:
    static void runMainThreadTasks(ServerNetworkHandler &owner) {
        std::vector<std::function<void()>> tasks;
        tasks.swap(owner.mMainThreadTasks);
        for (const std::function<void()> &task: tasks)
            task();
    }
};

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
                          + "\", \"uuid\": \"00000000-0000-4000-8000-700000000001\", \"version\": [1, 0, 0],"
                            " \"min_engine_version\": [1, 21, 0]},"
                            " \"modules\": [{\"type\": \"script\", \"language\": \"javascript\","
                            " \"uuid\": \"00000000-0000-4000-9000-700000000001\", \"entry\": \"scripts/main.js\","
                            " \"version\": [1, 0, 0]}],"
                            " \"dependencies\": [{\"module_name\": \"@minecraft/server\", \"version\": \"beta\"}]}\n");
        writeFile(root / name / "scripts" / "main.js", script);
    }

    const char *const MODULE_UUID = "00000000-0000-4000-9000-700000000001";

    const char *const JAR_BLOCK = R"JSON({
  "format_version": "1.21.90",
  "minecraft:block": {
    "description": { "identifier": "test:jar" },
    "components": { "minecraft:block_entity": { "entity_storage": true } }
  }
})JSON";

    const char *const SCRIPT = R"JS(
import { world, system, BlockEntityStorageComponent, BlockEntityStorageInfo } from "@minecraft/server";

const r = {};
const save = () => world.setDynamicProperty("storage", JSON.stringify(r));
const fail = (f) => { try { f(); return "ok"; } catch (e) { return e.name + ":" + e.message; } };
const dimension = () => world.getDimension("overworld");
const at = (x, z) => dimension().getBlock({ x, y: 100, z });

system.afterEvents.scriptEventReceive.subscribe((event) => {
    if (event.id === "storage:store") {
        const jar = at(1, 1).getComponent("entity_storage");
        r.component = (jar instanceof BlockEntityStorageComponent) + ":" + jar.typeId + ":"
            + BlockEntityStorageComponent.componentId + ":" + jar.size();
        const pig = dimension().spawnEntity("minecraft:pig", { x: 4.5, y: 100, z: 4.5 });
        const id = jar.store(pig);
        r.stored = (id === pig.id) + ":" + jar.size();
        const infos = jar.getEntities();
        r.infos = infos.length + ":" + (infos[0] instanceof BlockEntityStorageInfo) + ":" + infos[0].definitionId + ":"
            + (infos[0].uniqueId === id);
        r.errors = [fail(() => jar.release("abc")), fail(() => jar.release("12345")),
            fail(() => jar.transfer(id, at(2, 2))), fail(() => jar.transfer(id, at(1, 1))),
            fail(() => jar.store(1)), fail(() => jar.release(7))].join("|");
        r.stoneStorage = String(at(2, 2).getComponent("entity_storage"));
        r.id = id;
        save();
    } else if (event.id === "storage:move") {
        const jar = at(1, 1).getComponent("entity_storage");
        const other = at(3, 3).getComponent("entity_storage");
        r.gone = String(dimension().getEntities({ type: "minecraft:pig" }).length);
        jar.transfer(r.id, at(3, 3));
        r.moved = jar.size() + ":" + other.size();
        const released = other.release(r.id);
        r.released = released.typeId + ":" + (released.id === r.id) + ":" + other.size() + ":"
            + Math.round(released.location.x * 10) / 10 + "," + Math.round(released.location.z * 10) / 10;
        for (let i = 0; i < 4; ++i)
            jar.store(dimension().spawnEntity("minecraft:pig", { x: 5.5, y: 100, z: 5.5 }));
        r.full = fail(() => jar.store(dimension().spawnEntity("minecraft:pig", { x: 6.5, y: 100, z: 6.5 }))) + ":"
            + jar.size();
        save();
    }
});
)JS";

    std::unique_ptr<json::Value> result(ServerNetworkHandler &owner) {
        const auto it = owner.getWorldDynamicProperties().find("storage");
        require(it != owner.getWorldDynamicProperties().end(), "no result from the storage pack");
        std::unique_ptr<json::Value> parsed = json::parse(it->second.mString);
        require(parsed != nullptr && parsed->isObject(), "the storage pack wrote no JSON");
        return parsed;
    }

    void expect(const json::Value &values, const char *key, const std::string &wanted) {
        const json::Value *value = values.get(key);
        require(value != nullptr, std::string("missing result ") + key);
        const std::string actual = value->mType == json::Value::Type::Boolean ? (value->mBoolean ? "true" : "false")
                                                                              : value->string();
        require(actual == wanted, std::string(key) + ": expected \"" + wanted + "\", got \"" + actual + "\"");
    }

    // without upcoming_creator_features the game rejects a block storing entities
    void checkStorageNeedsExperiment(const std::filesystem::path &directory) {
        const std::filesystem::path packs = directory / "gated";
        writePack(packs, "gated", "");
        writeFile(packs / "gated" / "blocks" / "jar.json", JAR_BLOCK);
        ServerNetworkHandler probe("Script block storage gate", "", 1);
        BehaviorPackManager manager;
        manager.discover(packs.string());
        CustomContentRegistry &registry = CustomContentRegistry::getInstance();
        registry.reset();
        registry.setUpcomingCreatorFeatures(false);
        registry.load(manager, probe.getItemDefinitions(), probe.getBlockDefinitions());
        require(registry.findBlock("test:jar") == nullptr, "an entity_storage block loaded without the experiment");
        registry.reset();
        registry.setUpcomingCreatorFeatures(true);
        registry.load(manager, probe.getItemDefinitions(), probe.getBlockDefinitions());
        const CustomBlockDefinition *jar = registry.findBlock("test:jar");
        require(jar != nullptr && jar->mEntityStorage, "entity_storage was not read");
        registry.reset();
    }

    // the saved tag round-trips: buckets by module, the entity save with UniqueID and Definition
    void checkStorageRecord() {
        BlockEntityStorage storage;
        Tag pig = Tag::ofCompound();
        pig.putLong("UniqueID", 42);
        pig.putString("Definition", "minecraft:pig");
        storage.add("module", pig);
        Tag data = Tag::ofCompound();
        storage.save(data);
        const Tag *buckets = data.get("BlockEntityStorage");
        require(buckets != nullptr && buckets->isList() && buckets->getList().size() == 1
                        && buckets->getList()[0].getString("Bucket") == "module",
                "the storage did not save one bucket");
        BlockEntityStorage restored;
        restored.load(data);
        require(restored.contains(42) && restored.total() == 1, "the storage did not load its entity");
        Tag taken;
        require(restored.take("module", 42, taken) && taken.getString("Definition") == "minecraft:pig",
                "the stored entity did not come out");
        Tag empty = data;
        restored.save(empty);
        require(!empty.contains("BlockEntityStorage"), "an empty storage kept its tag");
    }

    void run(const std::filesystem::path &directory) {
        checkStorageRecord();
        checkStorageNeedsExperiment(directory);

        const std::filesystem::path packs = directory / "packs";
        writePack(packs, "storage", SCRIPT);
        writeFile(packs / "storage" / "blocks" / "jar.json", JAR_BLOCK);
        CustomContentRegistry::getInstance().setUpcomingCreatorFeatures(true);

        ServerNetworkHandler owner("Script block storage", "", 1);
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);
        BehaviorPackManager manager;
        manager.discover(packs.string());
        CustomContentRegistry::getInstance().load(manager, owner.getItemDefinitions(), owner.getBlockDefinitions());
        require(engine.loadPacks(manager.getPacks()) == 1, "the storage pack did not start");
        engine.startup();

        World &world = owner.getWorlds().getDefault();
        Level &level = world.getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        level.setBlock(Vector3i(1, 100, 1), BlockState("test:jar"), false);
        level.setBlock(Vector3i(2, 100, 2), BlockState("minecraft:stone"), false);
        level.setBlock(Vector3i(3, 100, 3), BlockState("test:jar"), false);
        engine.tick(0);

        engine.emitScriptEvent("storage:store", "");
        ServerTickProbe::runMainThreadTasks(owner);

        BlockActor *jar = level.getBlockActors().find(Vector3i(1, 100, 1));
        require(jar != nullptr && std::string(jar->getBlockActorId()) == "DataDriven",
                "the jar has no data-driven block actor");
        const Tag saved = jar->saveWithPosition();
        const Tag *buckets = saved.get("BlockEntityStorage");
        require(buckets != nullptr && buckets->isList() && buckets->getList().size() == 1, "the jar saved no bucket");
        const Tag &bucket = buckets->getList()[0];
        const Tag *entities = bucket.get("Entities");
        require(bucket.getString("Bucket") == MODULE_UUID && entities != nullptr && entities->isList()
                        && entities->getList().size() == 1
                        && entities->getList()[0].getString("Definition") == "minecraft:pig",
                "the bucket is not keyed by the module or lost the pig");

        engine.emitScriptEvent("storage:move", "");
        ServerTickProbe::runMainThreadTasks(owner);

        const std::unique_ptr<json::Value> r = result(owner);
        expect(*r, "component", "true:minecraft:entity_storage:minecraft:entity_storage:0");
        expect(*r, "stored", "true:1");
        expect(*r, "gone", "0");
        expect(*r, "infos", "1:true:minecraft:pig:true");
        expect(*r, "errors",
               "Error:Invalid entity ID.|Error:The entity was not found in storage.|"
               "Error:The target block does not have entity storage.|"
               "Error:The target block is the same entity storage.|TypeError:Native type conversion failed.|"
               "TypeError:Native type conversion failed.");
        expect(*r, "stoneStorage", "undefined");
        expect(*r, "moved", "0:1");
        expect(*r, "released", "minecraft:pig:true:0:4.5,4.5");
        expect(*r, "full", "Error:Entity storage is full.:4");

        // a pack block has no class: its block actor leaves once the block is replaced
        level.setBlock(Vector3i(3, 100, 3), BlockState("minecraft:stone"), false);
        // owner.tick returns at once while the server does not listen: the block actor step of a tick
        level.getBlockActors().tick(owner);
        require(level.getBlockActors().find(Vector3i(3, 100, 3)) == nullptr, "the replaced jar kept its block actor");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-script-block-storage-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Script block storage regression failed: " << error.what() << '\n';
        result = 1;
    }
    CustomContentRegistry::getInstance().reset();
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
