#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/TextPacket.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/Binding/ScriptApi.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/StructureBlockActor.h"
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

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void writeFile(const std::filesystem::path &path, const std::string &contents) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << contents;
    }

    void writePack(const std::filesystem::path &root, const std::string &name, const std::string &dependencies,
                   const std::string &script) {
        writeFile(root / name / "manifest.json",
                  "{\"format_version\": 2, \"header\": {\"name\": \"" + name
                          + "\", \"uuid\": \"00000000-0000-4000-8000-500000000001\", \"version\": [1, 0, 0],"
                            " \"min_engine_version\": [1, 21, 0]},"
                            " \"modules\": [{\"type\": \"script\", \"language\": \"javascript\","
                            " \"uuid\": \"00000000-0000-4000-9000-500000000001\", \"entry\": \"scripts/main.js\","
                            " \"version\": [1, 0, 0]}],"
                            " \"dependencies\": ["
                          + dependencies + "]}\n");
        writeFile(root / name / "scripts" / "main.js", script);
    }

    const char *const GAMETEST = R"JS(
import { world, system, BlockPermutation } from "@minecraft/server";
import * as gt from "@minecraft/server-gametest";

const r = {};
const errorOf = (run) => { try { run(); return "ok"; } catch (e) { return e.name + ":" + e.message; } };
r.builder = errorOf(() => gt.register("Suite", "passes", (test) => {
    test.setBlockType("minecraft:stone", { x: 1, y: 1, z: 1 });
    test.assertBlockPresent("minecraft:stone", { x: 1, y: 1, z: 1 });
    r.world = r.world || JSON.stringify(test.worldBlockLocation({ x: 1, y: 1, z: 1 }));
    r.relative = JSON.stringify(test.relativeBlockLocation(test.worldBlockLocation({ x: 2, y: 1, z: 0 })));
    r.direction = test.getTestDirection();
    test.succeed();
    r.done = test.isCompleted() + "|" + errorOf(() => test.succeed());
}).structureName("gt:box").tag("suite:default").maxTicks(40));
gt.register("Suite", "fails", (test) => {
    try {
        test.assertBlockPresent("minecraft:diamond_block", { x: 0, y: 1, z: 0 });
    } catch (e) {
        r.error = (e instanceof gt.GameTestError) + "|" + e.type + "|" + e.message + "|" + e.messageParameters
            + "|" + JSON.stringify(e.context.relativePosition);
        throw e;
    }
}).structureName("gt:box").required(false).tag("suite:default");
gt.register("Suite", "times", (test) => {}).structureName("gt:box").maxTicks(3);
gt.register("Suite", "sequence", (test) => {
    test.startSequence().thenIdle(2).thenExecute(() => { r.sequence = test.isCompleted(); }).thenSucceed();
}).structureName("gt:box");
gt.registerAsync("Suite", "async", async (test) => {
    await test.idle(2);
    test.succeed();
}).structureName("gt:box").tag("suite:default");
gt.register("Suite", "missing", (test) => test.succeed()).structureName("gt:nothing");
gt.setBeforeBatchCallback("", () => { r.before = String(Number(r.before || 0) + 1); });
r.twice = errorOf(() => gt.setBeforeBatchCallback("", () => {}));
r.tags = gt.Tags.suiteDefault + "|" + gt.Tags.suiteAll;
r.registrationBuilder = errorOf(() => new gt.RegistrationBuilder());

system.afterEvents.scriptEventReceive.subscribe((event) => {
    if (event.id === "gt:prepare")
        world.structureManager.createEmpty("gt:box", { x: 3, y: 3, z: 3 });
    if (event.id === "gt:report")
        world.setDynamicProperty("gametest", JSON.stringify(r));
});
)JS";

    std::unique_ptr<json::Value> result(ServerNetworkHandler &owner, const std::string &key) {
        const auto it = owner.getWorldDynamicProperties().find(key);
        require(it != owner.getWorldDynamicProperties().end(), "no result from the " + key + " pack");
        std::unique_ptr<json::Value> parsed = json::parse(it->second.mString);
        require(parsed != nullptr && parsed->isObject(), "the " + key + " pack wrote no JSON");
        return parsed;
    }

    void expect(const json::Value &values, const char *key, const std::string &wanted) {
        const json::Value *value = values.get(key);
        require(value != nullptr, std::string("missing result ") + key);
        std::string actual = value->mString;
        if (value->mType == json::Value::Type::Boolean)
            actual = value->mBoolean ? "true" : "false";
        require(actual == wanted, std::string(key) + ": expected \"" + wanted + "\", got \"" + actual + "\"");
    }

    // the chat lines a few ticks send, joined
    std::string tickAndRead(ServerNetworkHandler &owner, ScriptEngine &engine, int64_t &tick, int32_t ticks,
                            const std::function<void()> &first = nullptr) {
        std::string chat;
        owner.getNetworkHandler().setOutboundFilter([&chat, &owner](const NetworkIdentifier &, std::string &data) {
            ReadOnlyBinaryStream header(data);
            unsigned char sender = 0, client = 0;
            if (Packet::peekId(header, sender, client) == MinecraftPacketIds::Text) {
                ReadOnlyBinaryStream body(data);
                TextPacket packet;
                packet.readHeader(body);
                packet.read(body, owner.getCodecContext());
                chat += packet.mMessage + "\n";
            }
            return false;
        });
        if (first)
            first();
        for (int32_t index = 0; index < ticks; ++index)
            engine.tick(++tick);
        owner.getNetworkHandler().setOutboundFilter(nullptr);
        return chat;
    }

    bool contains(const std::string &text, const std::string &part) {
        return text.find(part) != std::string::npos;
    }

    void run(const std::filesystem::path &directory) {
        const std::filesystem::path packs = directory / "packs";
        writePack(packs, "gametest",
                  "{\"module_name\": \"@minecraft/server\", \"version\": \"2.12.0-beta\"},"
                  " {\"module_name\": \"@minecraft/server-gametest\", \"version\": \"1.0.0-beta\"},"
                  " {\"module_name\": \"@minecraft/common\", \"version\": \"1.3.0\"}",
                  GAMETEST);

        ServerNetworkHandler owner("Script gametest", "", 1);
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);
        BehaviorPackManager manager;
        manager.discover(packs.string());
        CustomContentRegistry::getInstance().load(manager, owner.getItemDefinitions(), owner.getBlockDefinitions());
        require(engine.loadPacks(manager.getPacks()) == 1, "the gametest pack did not start");

        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 4001).first->second;
        engine.startup();
        Level &level = owner.getWorlds().getDefault().getOverworld();
        for (int32_t chunkX = -2; chunkX <= 4; ++chunkX) {
            for (int32_t chunkZ = -2; chunkZ <= 4; ++chunkZ)
                level.insertChunk(LevelChunk(chunkX, chunkZ));
        }
        const BlockState stone = ScriptApi::defaultBlockState("minecraft:stone");
        for (int32_t x = -16; x <= 70; ++x) {
            for (int32_t z = -16; z <= 70; ++z)
                level.setBlock(Vector3i(x, 99, z), stone, false);
        }
        player.setName("GameTestPlayer");
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setLoginState(ServerPlayer::LoginState::Spawned);

        int64_t tick = 0;
        engine.queueScriptEvent("gt:prepare", "");
        engine.tick(++tick);

        GameTestRunner &runner = engine.gameTests();
        require(runner.testNames().size() == 6, "the registered tests are not listed");
        require(!runner.run(level, Vector3i(0, 100, 0), "Suite:none", 0, false, 1).mSuccess, "an unknown test ran");
        require(runner.run(level, Vector3i(0, 100, 0), "Suite:passes", 0, false, 0).mKey
                        == "gameTest.error.repeatCountTooSmall",
                "a repeat count of 0 was accepted");

        // the structure block sits on the ground three blocks south, the structure over it
        require(runner.run(level, Vector3i(0, 100, 0), "suite:PASSES", 0, false, 1).mSuccess, "run failed");
        const StructureBlockActor *block = level.getBlockActors().find<StructureBlockActor>(Vector3i(0, 100, 3));
        require(block != nullptr && block->mStructureName == "gt:box" && block->mMode == StructureBlockActor::Mode::Load
                        && block->mOffset.y == 1 && block->mSize.x == 3,
                "the structure block is wrong");
        require(level.getBlockState(0, 99, 3).mName == "minecraft:redstone_block", "no redstone block under it");
        require(level.getBlockState(1, 100, 2).mName == "minecraft:command_block", "no command block");
        std::string chat = tickAndRead(owner, engine, tick, 2);
        require(contains(chat, "gameTest.success.passed") && contains(chat, "Suite:passes"), "no passed line: " + chat);
        require(contains(chat, "gameTest.report.testCount") && contains(chat, "gameTest.report.allRequiredTestsPassed"),
                "no summary: " + chat);
        require(level.getBlockState(1, 101, 4).mName == "minecraft:stone", "the test did not set its block");
        require(level.getBlockState(-1, 99, 2).mName == "minecraft:beacon"
                        && level.getBlockState(-1, 100, 2).mName == "minecraft:lime_stained_glass",
                "no lime beacon");

        require(runner.run(level, Vector3i(20, 100, 0), "Suite:times", 0, false, 1).mSuccess, "times did not run");
        chat = tickAndRead(owner, engine, tick, 6);
        require(contains(chat, "gameTest.error.timeout") && contains(chat, "gameTest.report.someTestsFailed"),
                "no timeout: " + chat);
        require(level.getBlockState(19, 100, 2).mName == "minecraft:red_stained_glass", "no red beacon");
        require(level.getBlockState(19, 101, 2).mName == "minecraft:lectern", "no report lectern");

        bool missingRan = false;
        chat = tickAndRead(owner, engine, tick, 1, [&]() {
            missingRan = runner.run(level, Vector3i(40, 100, 0), "Suite:missing", 0, false, 1).mSuccess;
        });
        require(missingRan, "missing not run");
        require(contains(chat, "gameTest.error.structureNotSpawned"), "no missing structure line: " + chat);

        require(runner.run(level, Vector3i(40, 100, 20), "Suite:sequence", 0, false, 1).mSuccess, "no sequence");
        chat = tickAndRead(owner, engine, tick, 5);
        require(contains(chat, "Suite:sequence"), "the sequence did not finish: " + chat);

        // the default suite: passes, fails (optional) and async, in one batch
        const GameTestRunner::Outcome set = runner.runSet(level, Vector3i(0, 100, 30), "", 0, false);
        require(set.mSuccess && set.mKey == "gameTest.success.testsStarted" && set.mParameters[0] == "3"
                        && set.mParameters[1] == "suite:default",
                "runset did not start the default suite");
        chat = tickAndRead(owner, engine, tick, 6);
        require(contains(chat, "gameTest.report.optionalFailures") && contains(chat, "gameTest.assert.expectBlock"),
                "no optional failure: " + chat);
        require(runner.runSet(level, Vector3i(0, 100, 30), "nope", 0, false).mKey == "gameTest.error.noTestsFound",
                "an unknown tag found tests");

        const GameTestRunner::Outcome created = runner.create(level, Vector3i(0, 100, 60), "Made:here", 4, 0, 0);
        require(created.mSuccess, "create failed");
        const StructureBlockActor *made = level.getBlockActors().find<StructureBlockActor>(Vector3i(0, 100, 63));
        require(made != nullptr && made->mMode == StructureBlockActor::Mode::Save && made->mSize.z == 4,
                "create made no save block");
        require(level.getBlockState(3, 101, 66).mName == "minecraft:polished_andesite", "no floor");
        require(runner.create(level, Vector3i(0, 100, 60), "Big", 49, 0, 0).mKey == "gameTest.error.structureTooBig",
                "a big structure was accepted");

        const GameTestRunner::Outcome where = runner.position(level, Vector3i(1, 101, 4));
        require(where.mSuccess && where.mParameters[0] == "gt:box" && where.mParameters[1] == "1, 1, 1",
                "pos is wrong");

        runner.clearAll(level);
        require(level.getBlockState(0, 100, 3).mName == "minecraft:air"
                        && level.getBlockState(-1, 99, 2).mName == "minecraft:air",
                "clearall left the test blocks");

        engine.queueScriptEvent("gt:report", "");
        engine.tick(++tick);
        const std::unique_ptr<json::Value> report = result(owner, "gametest");
        expect(*report, "builder", "ok");
        expect(*report, "world", "{\"x\":1,\"y\":101,\"z\":4}");
        expect(*report, "relative", "{\"x\":2,\"y\":1,\"z\":0}");
        expect(*report, "direction", "South");
        expect(*report, "done",
               "true|GameTestCompletedError:GameTest Suite:passes has already completed and can no "
               "longer access method: succeed");
        expect(*report, "error",
               "true|AssertAtPosition|gameTest.assert.expectBlock|minecraft:diamond_block|"
               "{\"x\":0,\"y\":1,\"z\":0}");
        expect(*report, "sequence", "false");
        expect(*report, "twice", "Error:GameTest batch  has already been assigned a BeforeBatchCallback.");
        expect(*report, "tags", "suite:default|suite:all");
        expect(*report, "registrationBuilder", "ReferenceError:No constructor for native class 'RegistrationBuilder'.");
        expect(*report, "before", "1");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-script-gametest-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Script gametest regression failed: " << error.what() << '\n';
        result = 1;
    }
    CustomContentRegistry::getInstance().reset();
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
