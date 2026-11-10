#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/ScriptEngine.h"
#include "Server/Commands/PlayerCommandOrigin.h"
#include "Server/Commands/ScriptEventCommand.h"
#include "Server/Commands/ServerCommandOrigin.h"
#include "Server/ServerPaths.h"
#include "World/Level/GameRules.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"
#include "World/Actor/MobEffect.h"
#include "World/Actor/Player/ServerPlayer.h"

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

    // a script pack: dependencies is the manifest's JSON array body
    void writePack(const std::filesystem::path &root, const std::string &name, const std::string &dependencies,
                   const std::string &script, const std::string &extra = std::string()) {
        static int serial = 0;
        ++serial;
        const std::string header = "00000000-0000-4000-8000-" + std::to_string(100000000000 + serial);
        const std::string module = "00000000-0000-4000-9000-" + std::to_string(100000000000 + serial);
        writeFile(root / name / "manifest.json",
                  "{\"format_version\": 2,\n"
                  " \"header\": {\"name\": \""
                          + name + "\", \"uuid\": \"" + header
                          + "\", \"version\": [1, 0, 0],"
                            " \"min_engine_version\": [1, 21, 0]},\n"
                            " // comments are allowed in manifests\n"
                            " \"modules\": [{\"type\": \"script\", \"language\": \"javascript\", \"uuid\": \""
                          + module
                          + "\","
                            " \"entry\": \"scripts/main.js\", \"version\": [1, 0, 0]}],\n"
                            " \"dependencies\": ["
                          + dependencies + "]" + extra + "}\n");
        writeFile(root / name / "scripts" / "main.js", script);
    }

    const char *const PRELUDE = R"JS(
const r = {};
const capture = (f) => {
    try {
        f();
        return "ok";
    } catch (e) {
        return e.name + ": " + e.message;
    }
};
globalThis["marker_" + Math.random()] = true;
r.alone = Object.keys(globalThis).filter((k) => k.startsWith("marker_")).length === 1;
r.internals = Object.getOwnPropertyNames(globalThis).filter((k) => k.startsWith("__falcon")).length;
)JS";

    const char *const STABLE2 = R"JS(
import { world, system, Entity, Player, ItemStack, Dimension, World, GameMode, InputPermissionCategory,
         PlayerInventoryType, EntityInventoryComponent, EntityComponentTypes, InvalidEntityError,
         ScreenDisplay, TickingAreaManager, Difficulty, TimeOfDay, MoonPhase, WeatherType, ScriptEventSource,
         GameRule, MemoryTier } from "@minecraft/server";
import * as server from "@minecraft/server";
import { EngineError, InvalidArgumentError, InvalidArgumentErrorType } from "@minecraft/common";

r.early = capture(() => world.getAllPlayers());
r.earlyRun = capture(() => system.run(() => {}));
r.entityConstructor = capture(() => new Entity());
r.playerExtendsEntity = Object.getPrototypeOf(Player.prototype) === Entity.prototype
    && Object.getPrototypeOf(Player) === Entity;
r.worldIsWorld = world instanceof World;
r.gameMode = GameMode.Survival + "," + GameMode.Spectator;
r.sneak = InputPermissionCategory.Sneak + "," + InputPermissionCategory.MoveRight;
r.inventoryType = PlayerInventoryType.Inventory + "," + PlayerInventoryType.Hotbar;
r.componentId = EntityInventoryComponent.componentId + "," + EntityComponentTypes.Inventory;
r.kick = "kick" in Player.prototype;
r.hideAllForms = "hideAllForms" in ScreenDisplay.prototype;
r.ticking = ("getAllTickingAreas" in TickingAreaManager.prototype) && !("getTickingAreas" in TickingAreaManager.prototype);
r.worldInitialize = "worldInitialize" in world.beforeEvents;
r.itemUseOn = "itemUseOn" in world.beforeEvents;
r.chatSend = ("chatSend" in world.afterEvents) || ("chatSend" in world.beforeEvents);
r.handle = Object.getOwnPropertyNames(Entity.prototype).filter((k) => k.startsWith("_")).length;
r.legacyExport = "WorldInitializeBeforeEvent" in server;
r.engineError = capture(() => new EngineError());
const argument = new InvalidArgumentError("f", "string", InvalidArgumentErrorType.Empty, 1);
r.invalidArgument = argument instanceof Error && argument.index === 1 && argument.type === "Empty";
r.enums = [Difficulty.Hard, TimeOfDay.Noon, MoonPhase.NewMoon, WeatherType.Thunder, ScriptEventSource.NPCDialogue,
           GameRule.KeepInventory, MemoryTier.SuperHigh].join(",");
r.signals = [world.afterEvents, world.beforeEvents, system.afterEvents, system.beforeEvents]
    .map((holder) => Object.keys(holder).filter((key) => typeof holder[key].subscribe === "function"
        && typeof holder[key].unsubscribe === "function").length).join(",");
r.v1Signals = ["gameRuleChange", "weatherChange"].every((key) => key in world.afterEvents)
    && "weatherChange" in world.beforeEvents && "scriptEventReceive" in system.afterEvents;

world.afterEvents.gameRuleChange.subscribe((event) => {
    r.gameRule = event.rule + "=" + event.value + "," + typeof event.value;
});
let weatherCalls = 0;
world.beforeEvents.weatherChange.subscribe((event) => {
    weatherCalls++;
    r.weatherBefore = (r.weatherBefore || "") + event.previousWeather + ">" + event.newWeather + ";";
    if (weatherCalls === 1) {
        event.cancel = true;
        return;
    }
    event.newWeather = WeatherType.Thunder;
    event.duration = 1234;
});
world.afterEvents.weatherChange.subscribe((event) => {
    r.weatherAfter = (r.weatherAfter || "") + [event.dimension, event.previousWeather, event.newWeather].join("|") + ";";
});
system.afterEvents.scriptEventReceive.subscribe((event) => {
    r.scriptEvents = (r.scriptEvents || "") + [event.id, event.message, event.sourceType,
        event.sourceEntity instanceof Player, event.initiator === undefined,
        event.sourceBlock === undefined].join("|") + ";";
});
const filtered = system.afterEvents.scriptEventReceive.subscribe((event) => {
    r.filtered = (r.filtered || "") + event.id + ";";
}, { namespaces: ["other"] });
const removed = (event) => { r.removed = event.id; };
system.afterEvents.scriptEventReceive.subscribe(removed, { namespaces: ["test"] });
system.afterEvents.scriptEventReceive.unsubscribe(removed);

let registry;
system.beforeEvents.startup.subscribe((event) => {
    r.startupEarly = capture(() => world.getAllPlayers());
    r.register = capture(() => event.blockComponentRegistry.registerCustomComponent("test:comp", {}));
    registry = event.blockComponentRegistry;
});

let saved;
world.beforeEvents.effectAdd.subscribe((event) => {
    saved = event.entity;
    r.restrictedWrite = capture(() => event.entity.addTag("x"));
    r.restrictedRead = capture(() => event.entity.getTags());
    r.restrictedRun = capture(() => system.run(() => {
        r.player = saved instanceof Player && saved instanceof Entity;
        r.same = world.getDimension("overworld") === world.getDimension("minecraft:overworld");
    }));
    r.amplifier = "amplifier" in event;
});

world.afterEvents.worldLoad.subscribe(() => {
    r.lateRegister = capture(() => registry.registerCustomComponent("test:late", {}));
    r.prototypeBound = capture(() => Entity.prototype.addTag("x"));
    r.normal = capture(() => world.getAllPlayers());
    r.item = new ItemStack("minecraft:stone", 2) instanceof ItemStack;
    r.dimension = world.getDimension("overworld") instanceof Dimension;
    world.setDynamicProperty("stable2", JSON.stringify(r));
});

system.runTimeout(() => {
    r.invalid = capture(() => saved.location);
    r.invalidClass = (() => {
        try {
            saved.location;
        } catch (e) {
            return e instanceof InvalidEntityError;
        }
        return false;
    })();
    r.isValid = saved.isValid;
    world.setDynamicProperty("stable2", JSON.stringify(r));
}, 6);

import("../../outside.js").then(() => { r.escape = "loaded"; }, () => { r.escape = "blocked"; });
)JS";

    const char *const WORLD = R"JS(
import { world, system, GameRules, TimeOfDay, MoonPhase, Difficulty, MemoryTier, NamespaceNameError } from "@minecraft/server";

r.rulesConstructor = capture(() => new GameRules());
r.earlyRules = capture(() => world.gameRules);
r.editor = system.isEditorWorld;
r.earlyTier = capture(() => system.serverSystemInfo.memoryTier);
r.earlySettings = capture(() => world.getPackSettings());
r.settings = JSON.stringify(world.getPackSettings());
system.waitTicks(2).then(() => { r.waited = true; });
r.waitZero = capture(() => system.waitTicks(0));

world.beforeEvents.effectAdd.subscribe(() => {
    r.restrictedRuleRead = capture(() => world.gameRules.pvp);
    r.restrictedRuleWrite = capture(() => { world.gameRules.pvp = false; });
    r.restrictedTime = capture(() => world.setTimeOfDay(TimeOfDay.Noon));
    r.restrictedDifficulty = capture(() => world.setDifficulty(Difficulty.Hard));
    r.restrictedSpawn = capture(() => world.setDefaultSpawnLocation({ x: 0, y: 64, z: 0 }));
});

system.runTimeout(() => {
    r.stableRules = world.gameRules === world.gameRules;
    r.memoryTier = system.serverSystemInfo.memoryTier === MemoryTier.High;
    r.pvp = world.gameRules.pvp;
    world.gameRules.pvp = false;
    world.gameRules.randomTickSpeed = 3;
    r.rules = world.gameRules.pvp + "," + world.gameRules.randomTickSpeed;
    r.ruleType = capture(() => { world.gameRules.keepInventory = 1; });
    world.setAbsoluteTime(24000 * 9 + 100);
    world.setTimeOfDay(TimeOfDay.Night);
    r.time = world.getAbsoluteTime() + "," + world.getTimeOfDay() + "," + world.getDay();
    r.moon = world.getMoonPhase() === MoonPhase.WaningGibbous;
    r.timeRange = capture(() => world.setTimeOfDay(24001));
    world.setDifficulty(Difficulty.Hard);
    r.difficulty = world.getDifficulty();
    world.setDefaultSpawnLocation({ x: 10, y: 70, z: -5 });
    const spawn = world.getDefaultSpawnLocation();
    r.spawn = spawn.x + "," + spawn.y + "," + spawn.z;
    r.spawnBounds = capture(() => world.setDefaultSpawnLocation({ x: 0, y: 5000, z: 0 }));
    r.players = world.getPlayers().length + "," + world.getPlayers({ name: "nobody" }).length;
    r.seed = typeof world.seed;
    r.hardcore = world.isHardcore;
    world.setDynamicProperties({ a: 1, b: "xy" });
    r.bytes = world.getDynamicPropertyTotalByteCount() > 0;
    r.noNamespace = capture(() => system.sendScriptEvent("noname", "m"));
    r.minecraftNamespace = (() => {
        try {
            system.sendScriptEvent("minecraft:x", "m");
        } catch (e) {
            return (e instanceof NamespaceNameError) + "," + e.reason;
        }
        return "ok";
    })();
    r.sendEvent = capture(() => system.sendScriptEvent("test:event", "m"));
}, 5);
system.runTimeout(() => world.setDynamicProperty("world", JSON.stringify(r)), 7);
)JS";

    const char *const STABLE1 = R"JS(
import { world, system, GameMode } from "@minecraft/server";
import * as server from "@minecraft/server";

r.gameMode = String(GameMode.survival) + "," + String(GameMode.Survival);
r.worldInitialize = "worldInitialize" in world.beforeEvents;
r.worldLoad = "worldLoad" in world.afterEvents;
r.systemBefore = "beforeEvents" in system;
r.early = capture(() => world.getAllPlayers());
r.startupEvent = "StartupEvent" in server;
world.beforeEvents.worldInitialize.subscribe((event) => {
    r.register = capture(() => event.blockComponentRegistry.registerCustomComponent("legacy:comp", {}));
});
world.afterEvents.worldInitialize.subscribe(() => {
    world.setDynamicProperty("stable1", JSON.stringify(r));
});
)JS";

    const char *const BETA = R"JS(
import { world } from "@minecraft/server";
import { ActionFormData } from "@minecraft/server-ui";

r.chatSend = ("chatSend" in world.afterEvents) && ("chatSend" in world.beforeEvents);
r.form = typeof ActionFormData === "function"
    && Object.getOwnPropertyNames(new ActionFormData()).length === 0;
world.afterEvents.worldLoad.subscribe(() => world.setDynamicProperty("beta", JSON.stringify(r)));
)JS";

    const char *const UNDECLARED = R"JS(
import { world } from "@minecraft/server";
import { ActionFormData } from "@minecraft/server-ui";
world.afterEvents.worldLoad.subscribe(() => world.setDynamicProperty("undeclared", "ran"));
)JS";

    const char *const NOT_IMPLEMENTED = R"JS(
import { world } from "@minecraft/server";
import { http } from "@minecraft/server-net";
world.afterEvents.worldLoad.subscribe(() => world.setDynamicProperty("net", "ran"));
)JS";

    const char *const NEVER_RUNS = R"JS(
import { world } from "@minecraft/server";
world.afterEvents.worldLoad.subscribe(() => world.setDynamicProperty("rejected", "ran"));
)JS";

    std::unique_ptr<json::Value> result(ServerNetworkHandler &owner, const std::string &key) {
        const auto it = owner.getWorldDynamicProperties().find(key);
        require(it != owner.getWorldDynamicProperties().end(), "no result from the " + key + " pack");
        std::unique_ptr<json::Value> parsed = json::parse(it->second.mString);
        require(parsed != nullptr && parsed->isObject(), "the " + key + " pack wrote no JSON");
        return parsed;
    }

    std::string text(const json::Value &values, const char *key) {
        const json::Value *value = values.get(key);
        require(value != nullptr, std::string("missing result ") + key);
        if (value->mType == json::Value::Type::Boolean)
            return value->mBoolean ? "true" : "false";
        if (value->isNumber())
            return std::to_string(value->integer(0));
        return value->string();
    }

    void expect(const json::Value &values, const char *key, const std::string &wanted) {
        const std::string actual = text(values, key);
        require(actual == wanted, std::string(key) + ": expected \"" + wanted + "\", got \"" + actual + "\"");
    }

    void run(const std::filesystem::path &directory) {
        const std::filesystem::path packs = directory / "packs";
        const std::string server2 = "{\"module_name\": \"@minecraft/server\", \"version\": \"2.11.0\"}";
        writePack(packs, "stable2", server2 + ", {\"module_name\": \"@minecraft/common\", \"version\": \"1.3.0\"}",
                  std::string(PRELUDE) + STABLE2);
        writePack(packs, "stable1", "{\"module_name\": \"@minecraft/server\", \"version\": \"1.19.0\"}",
                  std::string(PRELUDE) + STABLE1);
        writePack(packs, "beta",
                  "{\"module_name\": \"@minecraft/server\", \"version\": \"beta\"},"
                  " {\"module_name\": \"@minecraft/server-ui\", \"version\": \"2.4.0-beta\"}",
                  std::string(PRELUDE) + BETA);
        writePack(
                packs, "world", server2 + ", {\"module_name\": \"@minecraft/common\", \"version\": \"1.3.0\"}",
                std::string(PRELUDE) + WORLD,
                ", \"settings\": [{\"type\": \"label\", \"text\": \"x\"},"
                " {\"type\": \"toggle\", \"name\": \"test:on\", \"default\": true},"
                " {\"type\": \"slider\", \"name\": \"test:level\", \"default\": 4, \"min\": 0, \"max\": 8, \"step\": 1}]");
        writePack(packs, "undeclared", server2, UNDECLARED);
        writePack(packs, "net", server2 + ", {\"module_name\": \"@minecraft/server-net\", \"version\": \"1.0.0-beta\"}",
                  NOT_IMPLEMENTED);
        writePack(packs, "badversion", "{\"module_name\": \"@minecraft/server\", \"version\": \"2.99.0\"}", NEVER_RUNS);
        writePack(packs, "unknown",
                  server2 + ", {\"module_name\": \"@minecraft/server-unknown\", \"version\": \"1.0.0\"}", NEVER_RUNS);
        writeFile(packs / "outside.js", "export const x = 1;\n");

        ServerNetworkHandler owner("Script API", "", 1);
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);

        BehaviorPackManager manager;
        manager.discover(packs.string());
        require(engine.loadPacks(manager.getPacks()) == 4, "only the four valid packs start");

        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 1001).first->second;
        engine.startup();

        MobEffectInstance effect;
        effect.mId = MobEffectId::Speed;
        effect.mDuration = 20;
        engine.beforeEffectAdd(player, effect);

        World &world = owner.getWorlds().getDefault();
        owner.changeGameRule(world, "keepinventory", "true");
        owner.changeGameRule(world, "randomtickspeed", "3");
        owner.setRaining(world, true);
        require(!world.getOverworld().isRaining(), "a cancelled weatherChange still changed the weather");
        owner.setRaining(world, true);
        require(world.getOverworld().isThundering() && world.getOverworld().getRainTime() == 1234,
                "weatherChange did not apply the new weather and duration");

        ScriptEventCommand scriptEvent(owner);
        ServerCommandOrigin console(&owner);
        scriptEvent.execute(console, {"test:ping", "hello", "world"});
        PlayerCommandOrigin playerOrigin(owner, player, CommandOriginData());
        scriptEvent.execute(playerOrigin, {"other:pong"});
        require(!scriptEvent.execute(console, {"minecraft:nope", "x"}), "a minecraft namespace was accepted");
        require(!scriptEvent.execute(console, {"nonamespace"}), "an id without namespace was accepted");
        for (int64_t tick = 1; tick <= 3; ++tick)
            engine.tick(tick);

        owner.getPlayers().erase(id);
        for (int64_t tick = 4; tick <= 8; ++tick)
            engine.tick(tick);

        const std::unique_ptr<json::Value> stable2 = result(owner, "stable2");
        expect(*stable2, "alone", "true");
        expect(*stable2, "internals", "0");
        expect(*stable2, "early",
               "ReferenceError: Native function [World::getAllPlayers] cannot be used in early execution.");
        expect(*stable2, "earlyRun", "ok");
        expect(*stable2, "entityConstructor", "ReferenceError: No constructor for native class 'Entity'.");
        expect(*stable2, "playerExtendsEntity", "true");
        expect(*stable2, "worldIsWorld", "true");
        expect(*stable2, "gameMode", "Survival,Spectator");
        expect(*stable2, "sneak", "5,12");
        expect(*stable2, "inventoryType", "Inventory,Hotbar");
        expect(*stable2, "componentId", "minecraft:inventory,minecraft:inventory");
        expect(*stable2, "kick", "false");
        expect(*stable2, "hideAllForms", "false");
        expect(*stable2, "ticking", "true");
        expect(*stable2, "worldInitialize", "false");
        expect(*stable2, "itemUseOn", "false");
        expect(*stable2, "chatSend", "false");
        expect(*stable2, "handle", "0");
        expect(*stable2, "legacyExport", "false");
        expect(*stable2, "engineError", "ReferenceError: No constructor for native class 'EngineError'.");
        expect(*stable2, "invalidArgument", "true");
        expect(*stable2, "enums", "Hard,6000,4,Thunder,NPCDialogue,keepInventory,4");
        expect(*stable2, "signals", "65,15,1,2");
        expect(*stable2, "v1Signals", "true");
        expect(*stable2, "gameRule", "randomTickSpeed=3,number");
        expect(*stable2, "weatherBefore", "Clear>Rain;Clear>Rain;");
        expect(*stable2, "weatherAfter", "minecraft:overworld|Clear|Thunder;");
        expect(*stable2, "scriptEvents",
               "test:ping|hello world|Server|false|true|true;other:pong||Entity|true|true|true;test:event|m|Server|false|true|true;");
        expect(*stable2, "filtered", "other:pong;");
        require(stable2->get("removed") == nullptr, "an unsubscribed scriptEventReceive callback ran");
        expect(*stable2, "prototypeBound",
               "ReferenceError: Native function [Entity::addTag] object bound to prototype does not exist.");
        expect(*stable2, "startupEarly",
               "ReferenceError: Native function [World::getAllPlayers] cannot be used in early execution.");
        expect(*stable2, "register", "ok");
        expect(*stable2, "lateRegister",
               "CustomComponentInvalidRegistryError: Cannot register custom component "
               "outside of the ModuleStartup event.");
        expect(*stable2, "normal", "ok");
        expect(*stable2, "item", "true");
        expect(*stable2, "dimension", "true");
        expect(*stable2, "restrictedWrite",
               "ReferenceError: Native function [Entity::addTag] cannot be used in restricted execution.");
        expect(*stable2, "restrictedRead", "ok");
        expect(*stable2, "restrictedRun", "ok");
        expect(*stable2, "amplifier", "false");
        expect(*stable2, "player", "true");
        expect(*stable2, "same", "true");
        expect(*stable2, "escape", "blocked");
        expect(*stable2, "invalid",
               "InvalidEntityError: Failed to get property 'location' due to Entity being "
               "invalid (has the Entity been removed?).");
        expect(*stable2, "invalidClass", "true");
        expect(*stable2, "isValid", "false");

        const std::unique_ptr<json::Value> worldResult = result(owner, "world");
        const json::Value &w = *worldResult;
        expect(w, "rulesConstructor", "ReferenceError: No constructor for native class 'GameRules'.");
        expect(w, "stableRules", "true");
        expect(w, "earlyRules",
               "ReferenceError: Native property getter [World::gameRules] cannot be used in early execution.");
        expect(w, "editor", "false");
        expect(w, "memoryTier", "true");
        expect(w, "earlyTier",
               "ReferenceError: Native property getter [SystemInfo::memoryTier] cannot be used in early execution.");
        expect(w, "earlySettings", "ok");
        expect(w, "settings", "{\"test:on\":true,\"test:level\":4}");
        expect(w, "waitZero", "EngineError: ");
        expect(w, "waited", "true");
        expect(w, "restrictedRuleRead", "ok");
        expect(w, "restrictedRuleWrite",
               "ReferenceError: Native property setter [GameRules::pvp] cannot be used in restricted execution.");
        expect(w, "restrictedTime",
               "ReferenceError: Native function [World::setTimeOfDay] cannot be used in restricted execution.");
        expect(w, "restrictedDifficulty",
               "ReferenceError: Native function [World::setDifficulty] cannot be used in restricted execution.");
        expect(w, "restrictedSpawn",
               "ReferenceError: Native function [World::setDefaultSpawnLocation] cannot be used in restricted "
               "execution.");
        expect(w, "pvp", "true");
        expect(w, "rules", "false,3");
        expect(w, "ruleType", "TypeError: Native type conversion failed.");
        expect(w, "time", "229000,13000,9");
        expect(w, "moon", "true");
        expect(w, "timeRange", "Error: ");
        expect(w, "difficulty", "Hard");
        expect(w, "spawn", "10,70,-5");
        expect(w, "spawnBounds", "LocationOutOfWorldBoundariesError: ");
        expect(w, "players", "0,0");
        expect(w, "seed", "string");
        expect(w, "hardcore", "false");
        expect(w, "bytes", "true");
        expect(w, "noNamespace", "NamespaceNameError: ");
        expect(w, "minecraftNamespace", "true,DisallowedNamespace");
        expect(w, "sendEvent", "ok");
        const GameRules &rules = owner.getWorlds().getDefault().getOverworld().getGameRules();
        require(!rules.getBool("pvp") && rules.getInt("randomtickspeed") == 3, "game rules not written");

        const std::unique_ptr<json::Value> stable1 = result(owner, "stable1");
        expect(*stable1, "alone", "true");
        expect(*stable1, "gameMode", "survival,undefined");
        expect(*stable1, "worldInitialize", "true");
        expect(*stable1, "worldLoad", "false");
        expect(*stable1, "systemBefore", "false");
        expect(*stable1, "early", "ok");
        expect(*stable1, "startupEvent", "false");
        expect(*stable1, "register", "ok");

        const std::unique_ptr<json::Value> beta = result(owner, "beta");
        expect(*beta, "alone", "true");
        expect(*beta, "chatSend", "true");
        expect(*beta, "form", "true");

        const auto &properties = owner.getWorldDynamicProperties();
        require(properties.find("undeclared") == properties.end(), "an undeclared module was importable");
        require(properties.find("net") == properties.end(), "an unimplemented module was importable");
        require(properties.find("rejected") == properties.end(), "a pack with an invalid manifest ran");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-scripts-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Script API regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
