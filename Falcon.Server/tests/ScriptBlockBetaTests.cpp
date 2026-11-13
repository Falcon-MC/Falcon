#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/BlockTickingQueue.h"
#include "World/Level/Level.h"
#include "World/Level/NamedTickQueue.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

// tick() returns at once while the server does not listen: the steps of a tick these events need, in its order
class ServerTickProbe {
public:
    static void tick(ServerNetworkHandler &owner, Level &level) {
        ++owner.mCurrentTick;
        level.tickBlockUpdates();
        owner.getScriptEngine().tick(owner.mCurrentTick);
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

    const char *const MODULE_UUID = "00000000-0000-4000-9000-500000000001";

    void writePack(const std::filesystem::path &root, const std::string &name, const std::string &script) {
        writeFile(root / name / "manifest.json",
                  "{\"format_version\": 2, \"header\": {\"name\": \"" + name
                          + "\", \"uuid\": \"00000000-0000-4000-8000-500000000001\", \"version\": [1, 0, 0],"
                            " \"min_engine_version\": [1, 21, 0]},"
                            " \"modules\": [{\"type\": \"script\", \"language\": \"javascript\","
                            " \"uuid\": \""
                          + MODULE_UUID
                          + "\", \"entry\": \"scripts/main.js\", \"version\": [1, 0, 0]}],"
                            " \"dependencies\": [{\"module_name\": \"@minecraft/server\", \"version\": \"beta\"}]}\n");
        writeFile(root / name / "scripts" / "main.js", script);
    }

    const char *const TIMER_BLOCK = R"JSON({
  "format_version": "1.21.90",
  "minecraft:block": {
    "description": { "identifier": "test:timer" },
    "components": { "test:named": {} }
  }
})JSON";

    const char *const WATCHER_BLOCK = R"JSON({
  "format_version": "1.21.90",
  "minecraft:block": {
    "description": { "identifier": "test:watcher" },
    "components": { "test:watch": {}, "minecraft:neighbor_change": { "neighbor_directions": ["north", "up"] } }
  }
})JSON";

    const char *const SCRIPT = R"JS(
import { world, system, BlockComponentNamedTickEvent, BlockComponentNeighborChangedAfterEvent }
    from "@minecraft/server";

const r = {};
const save = () => world.setDynamicProperty("blockbeta", JSON.stringify(r));
const fail = (f) => { try { f(); return "ok"; } catch (e) { return e.name + ":" + e.message; } };

system.beforeEvents.startup.subscribe((event) => {
    event.blockComponentRegistry.registerCustomComponent("test:named", {
        onNamedTick(e) {
            r.fired = (r.fired || "") + (system.currentTick - r.start) + ":" + e.isName("ping") + ":"
                + e.isName("pong") + ":" + e.block.typeId + ":" + (e instanceof BlockComponentNamedTickEvent) + ";";
            save();
        },
    });
    event.blockComponentRegistry.registerCustomComponent("test:watch", {
        onNeighborChanged(e) {
            r.neighbor = (r.neighbor || "") + e.getChanges().map((c) => c.direction + ":"
                + c.previousPermutation.type.id + ">" + c.blockPermutation.type.id).join(",") + "@" + e.block.typeId
                + ":" + (e instanceof BlockComponentNeighborChangedAfterEvent) + ";";
            save();
        },
    });
});

world.afterEvents.entityHitBlock.subscribe((e) => {
    r.hit = e.hitBlockPermutation.type.id + ":" + e.hitBlock.typeId + ":" + e.blockFace;
    save();
});

system.afterEvents.scriptEventReceive.subscribe((event) => {
    if (event.id !== "named:schedule")
        return;
    const block = world.getDimension("overworld").getBlock({ x: 1, y: 100, z: 1 });
    r.start = system.currentTick;
    block.scheduleNamedTick("ping", 3);
    block.scheduleNamedTick("pong", 5);
    r.has = block.hasScheduledNamedTick("ping") + ":" + block.hasScheduledNamedTick("nope");
    r.errors = [fail(() => block.scheduleNamedTick("", 1)), fail(() => block.scheduleNamedTick("x".repeat(32), 1)),
        fail(() => block.scheduleNamedTick("ok", 0)), fail(() => block.scheduleNamedTick("gone", 2)),
        fail(() => block.scheduleNamedTick("gone", 2)), fail(() => block.scheduleNamedTick("full", 2)),
        fail(() => block.scheduleNamedTick(1, 2))].join("|");
    block.removeScheduledNamedTick("gone");
    r.removed = block.hasScheduledNamedTick("gone");
    save();
});
)JS";

    std::unique_ptr<json::Value> result(ServerNetworkHandler &owner) {
        const auto it = owner.getWorldDynamicProperties().find("blockbeta");
        require(it != owner.getWorldDynamicProperties().end(), "no result from the block beta pack");
        std::unique_ptr<json::Value> parsed = json::parse(it->second.mString);
        require(parsed != nullptr && parsed->isObject(), "the block beta pack wrote no JSON");
        return parsed;
    }

    void expect(const json::Value &values, const char *key, const std::string &wanted) {
        const json::Value *value = values.get(key);
        require(value != nullptr, std::string("missing result ") + key);
        const std::string actual = value->mType == json::Value::Type::Boolean ? (value->mBoolean ? "true" : "false")
                                                                              : value->string();
        require(actual == wanted, std::string(key) + ": expected \"" + wanted + "\", got \"" + actual + "\"");
    }

    // the record keeps the events across a restart: the saved clock becomes the new level tick
    void checkNamedTickRecord() {
        NamedTickQueue queue;
        const Vector3i at(17, -60, 33);
        require(queue.schedule(at, "test:timer", "pack", "a", 100, 110) == NamedTickQueue::Result::Scheduled,
                "a valid event was refused");
        require(queue.schedule(at, "test:timer", "pack", "b", 100, 105) == NamedTickQueue::Result::Scheduled,
                "a second event was refused");
        require(queue.schedule(at, "minecraft:air", "pack", "c", 100, 105) == NamedTickQueue::Result::InvalidName,
                "an event on air was taken");
        require(queue.schedule(at, "test:timer", "", "c", 100, 105) == NamedTickQueue::Result::NoNamespace,
                "an event without namespace was taken");
        queue.tickColumn(1, 2, 100, [](const NamedTickQueue::Entry &, const std::string &) {});
        const std::string record = queue.save(1, 2, -64);
        require(!record.empty() && queue.save(0, 0, -64).empty(), "the record is not per chunk");

        NamedTickQueue restored;
        restored.load(1, 2, -64, record, 7);
        std::string order;
        restored.tickColumn(1, 2, 12, [&](const NamedTickQueue::Entry &entry, const std::string &eventNamespace) {
            order += entry.eventName + "@" + std::to_string(entry.triggerTime) + ":" + eventNamespace + ":"
                     + std::to_string(entry.position.x) + "," + std::to_string(entry.position.y) + ","
                     + std::to_string(entry.position.z) + ";";
        });
        require(order == "b@12:pack:17,-60,33;", "the restored events did not shift to the new clock: " + order);
        require(restored.has(at, "pack", "a") && !restored.has(at, "other", "a"), "the namespace was not kept");
        require(NamedTickQueue::hashNamespace("") == 0x811C9DC5u && NamedTickQueue::hashNamespace("a") == 0xE40C292Cu,
                "the namespace hash is not 32-bit FNV-1a");
    }

    ServerPlayer &addPlayer(ServerNetworkHandler &owner, Level &level, const char *name, const Vector3f &position) {
        static unsigned long long serial = 5000;
        ++serial;
        const NetworkIdentifier id(std::to_string(serial), serial);
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, serial).first->second;
        player.setName(name);
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.setPosition(position);
        return player;
    }

    // without upcoming_creator_features the game rejects a block using minecraft:neighbor_change
    void checkNeighborChangeNeedsExperiment(const std::filesystem::path &directory) {
        const std::filesystem::path packs = directory / "gated";
        writePack(packs, "gated", "");
        writeFile(packs / "gated" / "blocks" / "watcher.json", WATCHER_BLOCK);
        ServerNetworkHandler probe("Script block beta gate", "", 1);
        BehaviorPackManager manager;
        manager.discover(packs.string());
        CustomContentRegistry &registry = CustomContentRegistry::getInstance();
        registry.load(manager, probe.getItemDefinitions(), probe.getBlockDefinitions());
        require(registry.findBlock("test:watcher") == nullptr, "a neighbor_change block loaded without the experiment");
        registry.reset();
        registry.setUpcomingCreatorFeatures(true);
        registry.load(manager, probe.getItemDefinitions(), probe.getBlockDefinitions());
        require(registry.neighborDirectionsOf("test:watcher") == ((1 << 2) | (1 << 1)),
                "neighbor_directions did not map to north and up");
        registry.reset();
    }

    void run(const std::filesystem::path &directory) {
        checkNamedTickRecord();
        checkNeighborChangeNeedsExperiment(directory);

        const std::filesystem::path packs = directory / "packs";
        writePack(packs, "blockbeta", SCRIPT);
        writeFile(packs / "blockbeta" / "blocks" / "timer.json", TIMER_BLOCK);
        writeFile(packs / "blockbeta" / "blocks" / "watcher.json", WATCHER_BLOCK);
        CustomContentRegistry::getInstance().setUpcomingCreatorFeatures(true);

        ServerNetworkHandler owner("Script block beta", "", 1);
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);
        BehaviorPackManager manager;
        manager.discover(packs.string());
        CustomContentRegistry::getInstance().load(manager, owner.getItemDefinitions(), owner.getBlockDefinitions());
        require(engine.loadPacks(manager.getPacks()) == 1, "the block beta pack did not start");
        engine.startup();

        World &world = owner.getWorlds().getDefault();
        Level &level = world.getOverworld();
        // World::open hands the levels their owner; without it no neighbor change or named tick runs
        level.setOwner(&owner);
        level.insertChunk(LevelChunk(0, 0));
        // named ticks run in the columns players keep active
        level.setActiveColumns({BlockTickingQueue::column(0, 0)});
        ServerPlayer &player = addPlayer(owner, level, "Alex", Vector3f(0.5f, 100.0f, 0.5f));
        level.setBlock(Vector3i(1, 100, 1), BlockState("test:timer"), false);
        engine.tick(0);
        engine.onEntityHitBlock(player, Vector3i(1, 100, 1), 1);

        engine.emitScriptEvent("named:schedule", "");
        for (int tick = 0; tick < 8; ++tick)
            ServerTickProbe::tick(owner, level);

        // one event a tick: a direction keeps its first previous and last permutation, east is not watched
        level.setBlock(Vector3i(3, 100, 3), BlockState("test:watcher"), false);
        level.setBlock(Vector3i(3, 101, 3), BlockState("minecraft:stone"), false);
        level.setBlock(Vector3i(3, 100, 2), BlockState("minecraft:stone"), false);
        level.setBlock(Vector3i(3, 100, 2), BlockState("minecraft:dirt"), false);
        level.setBlock(Vector3i(4, 100, 3), BlockState("minecraft:stone"), false);
        ServerTickProbe::tick(owner, level);
        level.setBlock(Vector3i(4, 100, 3), BlockState("minecraft:air"), false);
        ServerTickProbe::tick(owner, level);
        level.setBlock(Vector3i(3, 101, 3), BlockState("minecraft:air"), false);
        ServerTickProbe::tick(owner, level);

        const std::unique_ptr<json::Value> r = result(owner);
        expect(*r, "neighbor",
               "Up:minecraft:air>minecraft:stone,North:minecraft:air>minecraft:dirt@test:watcher:true;"
               "Up:minecraft:stone>minecraft:air@test:watcher:true;");
        expect(*r, "has", "true:false");
        expect(*r, "errors",
               "Error:The scheduled event name is invalid.|Error:The scheduled event name is invalid.|"
               "ArgumentOutOfBoundsError:|ok|ok|Error:A block cannot have more than four scheduled events.|"
               "TypeError:Native type conversion failed.");
        expect(*r, "removed", "false");
        expect(*r, "hit", "test:timer:test:timer:Up");
        expect(*r, "fired", "3:true:false:test:timer:true;5:false:true:test:timer:true;");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-script-block-beta-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Script block beta regression failed: " << error.what() << '\n';
        result = 1;
    }
    CustomContentRegistry::getInstance().reset();
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
