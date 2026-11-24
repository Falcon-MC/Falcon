#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/PlayerAuthInputPacket.h"
#include "Protocol/Packets/TextPacket.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/Binding/ScriptApi.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/SculkCatalystBlockActor.h"
#include "World/Level/Block/Actor/StructureBlockActor.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

#include <chrono>
#include <cmath>
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
import { world, system, BlockPermutation, ItemStack, Player } from "@minecraft/server";
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
gt.register("Suite", "turns", (test) => test.succeed()).structureName("gt:box").rotateTest(true).tag("gt:turns").batch("turns");
gt.register("Suite", "cauldron", (test) => {
    const at = { x: 1, y: 1, z: 1 };
    test.setBlockType("minecraft:cauldron", at);
    test.setFluidContainer(at, "Lava");
    const liquid = test.getBlock(at).permutation.getState("cauldron_liquid");
    test.setBlockType("minecraft:stone", at);
    let error = "none";
    try {
        test.setFluidContainer(at, "Water");
    } catch (e) {
        error = e.type + "|" + e.message;
    }
    r.cauldron = liquid + "|" + error;
    test.succeed();
}).structureName("gt:box");
gt.register("Suite", "fuse", (test) => {
    const cow = test.spawn("minecraft:cow", { x: 1, y: 1, z: 1 });
    try {
        test.setTntFuse(cow, 5);
        r.fuse = "set";
    } catch (e) {
        r.fuse = e.type + "|" + e.message + "|" + e.messageParameters + "|" + JSON.stringify(e.context.relativePosition);
    }
    test.succeed();
}).structureName("gt:box");
gt.register("Suite", "sim", (test) => {
    const p = test.spawnSimulatedPlayer({ x: 1, y: 1, z: 1 }, "Bot");
    r.simType = (p instanceof gt.SimulatedPlayer) + "|" + (p instanceof Player) + "|" + p.name + "|"
        + (JSON.stringify(p.location) === JSON.stringify(test.worldLocation({ x: 1.5, y: 1, z: 1.5 })));
    r.simYaw = String(p.getRotation().y);
    p.setBodyRotation(45);
    r.simBody = String(p.getRotation().y);
    p.rotateBody(10);
    r.simRotate = String(p.getRotation().y);
    p.lookAtLocation({ x: 5, y: 2.62, z: 1.5 });
    r.simLook = String(p.headRotation.y);
    r.simJump = String(p.jump());
    r.simGlide = String(p.glide());
    p.swim();
    p.moveRelative(0, 1);
    const cow = test.spawnWithoutBehaviors("minecraft:cow", { x: 3, y: 1, z: 3 });
    r.simAttack = p.attackEntity(cow) + "|" + p.attack();
    r.simGive = p.giveItem(new ItemStack("minecraft:stone", 3), true) + "|" + p.setItem(new ItemStack("minecraft:dirt"), 5,
        true) + "|" + p.selectedSlotIndex + "|" + p.setItem(new ItemStack("minecraft:dirt"), 40);
    r.simDrop = p.dropSelectedItem() + "|" + p.dropSelectedItem();
    r.simRespawn = String(p.respawn());
    r.simBreak = String(p.breakBlock({ x: 1, y: 5, z: 1 }));
    r.simUsing = String(p.stopUsingItem());
    r.simInteract = String(p.interactWithBlock({ x: 1, y: 1, z: 1 }));
    r.simBuild = errorOf(() => { p.startBuild(); p.stopBuild(); p.stopInteracting(); });
    p.setSkin({ armSize: "Slim", skinColor: { red: 1, green: 0, blue: 0 } });
    const skin = gt.getPlayerSkin(p);
    r.simSkin = skin.armSize + "|" + skin.skinColor.red + "|" + skin.skinColor.green + "|" + skin.personaPieces.length;
    const nav = p.navigateToBlock({ x: 1, y: 1, z: 2 });
    r.simNav = (nav instanceof gt.NavigationResult) + "|" + typeof nav.isFullPath + "|" + Array.isArray(nav.getPath());
    r.simNavEntity = errorOf(() => p.navigateToEntity({}));
    p.moveRelative(0, 1);
    p.fly();
    r.simInvalid = errorOf(() => gt.SimulatedPlayer.prototype.jump.call({}));
    r.simEnums = gt.LookDuration.UntilMove + "|" + gt.PersonaArmSize.Slim + "|" + gt.PersonaPieceType.Skin;
    test.succeed();
}).structureName("gt:box");
// the catalyst is set and taken away by the server between the steps
gt.register("Suite", "sculk", (test) => {
    const cell = { x: 1, y: 1, z: 1 };
    const none = test.getSculkSpreader(cell);
    let s;
    test.runAfterDelay(5, () => {
        const at = test.worldBlockLocation(cell);
        s = test.getSculkSpreader(cell);
        s.addCursorsWithOffset({ x: 0, y: 1.7, z: -0.5 }, 2500);
        const first = s.getCursorPosition(0);
        r.sculk = String(none) + "|" + (s instanceof gt.SculkSpreader) + "|" + s.maxCharge + "|"
            + s.getNumberOfCursors() + "|" + s.getTotalCharge() + "|"
            + (first.x === at.x && first.y === at.y + 1 && first.z === at.z - 1) + "|"
            + JSON.stringify(s.getCursorPosition(3));
    });
    test.runAfterDelay(10, () => {
        r.sculkGone = s.maxCharge + "|" + s.getNumberOfCursors() + "|" + s.getTotalCharge() + "|"
            + String(test.getSculkSpreader(cell));
        test.succeed();
    });
}).maxTicks(20).structureName("gt:box");
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

    // charges past 1000 split, the delays and the facing are shorts, an empty spreader writes nothing
    void spreaderSaves() {
        SculkSpreader spreader;
        Tag empty = Tag::ofCompound();
        spreader.save(empty);
        require(empty.get("cursors") == nullptr, "an empty spreader saved cursors");
        spreader.addCursors(Vector3i(1, 2, 3), 1500);
        Tag saved = Tag::ofCompound();
        spreader.save(saved);
        const Tag *cursors = saved.get("cursors");
        require(cursors != nullptr && cursors->isList() && cursors->getList().size() == 2, "no cursor list");
        const Tag &second = cursors->getList()[1];
        require(second.getInt("y") == 2 && second.getShort("charge") == 500 && second.getShort("update") == 0
                        && second.getShort("decay") == 1 && second.getShort("facing") == -1,
                "the cursor was saved wrong");
        SculkSpreader loaded;
        loaded.load(saved);
        require(loaded.getCursors().size() == 2 && loaded.getTotalCharge() == 1500, "the cursors did not load");
    }

    void run(const std::filesystem::path &directory) {
        spreaderSaves();
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
        require(runner.testNames().size() == 11, "the registered tests are not listed");
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

        // a rotateTest test runs in all four rotations
        const GameTestRunner::Outcome turns = runner.runSet(level, Vector3i(0, 100, 45), "gt:turns", 0, false);
        require(turns.mSuccess && turns.mParameters[0] == "4", "rotateTest did not start four runs");
        chat = tickAndRead(owner, engine, tick, 4);
        require(contains(chat, "gameTest.report.testCount") && contains(chat, "\"4\""), "no four runs: " + chat);

        require(runner.run(level, Vector3i(40, 100, 45), "Suite:fuse", 0, false, 1).mSuccess, "fuse did not run");
        tickAndRead(owner, engine, tick, 2);
        require(runner.run(level, Vector3i(40, 100, 60), "Suite:cauldron", 0, false, 1).mSuccess,
                "cauldron did not run");
        tickAndRead(owner, engine, tick, 2);

        require(runner.run(level, Vector3i(60, 100, 20), "Suite:sculk", 0, false, 1).mSuccess, "sculk did not run");
        tickAndRead(owner, engine, tick, 3);
        const Vector3i catalyst(61, 101, 24);
        const BlockState catalystState = ScriptApi::defaultBlockState("minecraft:sculk_catalyst");
        level.setBlock(catalyst, catalystState, false);
        // the placement hooks run only for a level with its owner, which this harness leaves unset
        ServerNetworkHandler *previousOwner = level.getOwner();
        level.setOwner(&owner);
        level.onBlockPlaced(catalyst, catalystState);
        level.setOwner(previousOwner);
        require(level.getBlockActors().find<SculkCatalystBlockActor>(catalyst) != nullptr, "the catalyst has no actor");
        tickAndRead(owner, engine, tick, 5);
        level.setBlock(catalyst, ScriptApi::defaultBlockState("minecraft:air"), false);
        require(level.getBlockActors().find(catalyst) == nullptr, "the catalyst actor outlived its block");
        tickAndRead(owner, engine, tick, 6);

        // a simulated player: no client, its inputs come from its orders, nothing is sent to it
        require(runner.run(level, Vector3i(60, 100, 0), "Suite:sim", 0, false, 1).mSuccess, "sim did not run");
        tickAndRead(owner, engine, tick, 2);
        ServerPlayer *bot = nullptr;
        for (auto &entry: owner.getPlayers()) {
            if (entry.second.isSimulated())
                bot = &entry.second;
        }
        require(bot != nullptr && bot->getName() == "Bot" && bot->isSpawned(), "no simulated player");
        require(owner.getNetworkHandler().isLocalSession(bot->getNetworkIdentifier()), "the bot has a network session");
        PlayerAuthInputPacket input;
        bot->getSimulatedController()->buildInput(owner, *bot, input);
        require(input.mMotionX == 0.0f && input.mMotionY == 1.0f, "moveRelative did not walk forward");
        require(input.hasInputFlag((int32_t) PlayerAuthInputData::StartSwimming), "swim sent no swim input");
        require(bot->isFlying(), "fly did not set the flying ability");
        bot->getSimulatedController()->moveInDirection(Vector3f(1.0f, 0.0f, 0.0f), true);
        PlayerAuthInputPacket east;
        bot->getSimulatedController()->buildInput(owner, *bot, east);
        // facing east (yaw -90): east is straight ahead
        require(std::fabs(east.mMotionY - 1.0f) < 0.001f && std::fabs(east.mMotionX) < 0.001f,
                "a world move was not turned by the yaw");

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

        // the space of each structure: air from its lowest layer up, dirt under it
        runner.clearAll(level);
        require(level.getBlockState(1, 101, 4).mName == "minecraft:air", "clearall left the structure");
        require(level.getBlockState(0, 100, 3).mName == "minecraft:dirt"
                        && level.getBlockState(-1, 99, 2).mName == "minecraft:dirt"
                        && level.getBlockState(-1, 101, 2).mName == "minecraft:air",
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
        expect(*report, "cauldron", "lava|AssertAtPosition|gameTest.assert.expectCauldron");
        expect(*report, "simType", "true|true|Bot|true");
        expect(*report, "simYaw", "0");
        expect(*report, "simBody", "45");
        expect(*report, "simRotate", "55");
        expect(*report, "simLook", "-90");
        expect(*report, "simJump", "false");
        expect(*report, "simGlide", "false");
        expect(*report, "simInvalid",
               "ReferenceError:Native function [SimulatedPlayer::jump] object bound to prototype does not exist.");
        expect(*report, "simEnums", "UntilMove|Slim|Skin");
        expect(*report, "simAttack", "true|false");
        expect(*report, "simGive", "true|true|5|false");
        expect(*report, "simDrop", "true|false");
        expect(*report, "simRespawn", "false");
        expect(*report, "simBreak", "false");
        expect(*report, "simUsing", "undefined");
        expect(*report, "simInteract", "false");
        expect(*report, "simBuild", "ok");
        expect(*report, "simSkin", "Slim|1|0|0");
        expect(*report, "simNav", "true|boolean|true");
        expect(*report, "simNavEntity", "TypeError:Native type conversion failed.");
        expect(*report, "sculk", "undefined|true|1000|3|2500|true|{\"x\":0,\"y\":0,\"z\":0}");
        expect(*report, "sculkGone", "0|0|0|undefined");
        expect(*report, "fuse",
               "Assert|gameTest.assert.missingExplosionComponent|minecraft:cow|{\"x\":0,\"y\":0,\"z\":0}");
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
        // the server writes a restrictive default module list when there is none: these packs use every module
        writeFile(directory / "config" / "default" / "permissions.json", R"({"allowed_modules": ["*"]})");
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
