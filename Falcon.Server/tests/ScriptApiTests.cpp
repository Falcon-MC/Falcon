#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Core/Event/EventBus.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "Server/Commands/PlayerCommandOrigin.h"
#include "Server/Commands/ScriptEventCommand.h"
#include "Server/Commands/ServerCommandOrigin.h"
#include "Server/ServerPaths.h"
#include "World/Level/GameRules.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "Scripting/WorldClockSystem.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Network/Handler/LoginHandler.h"
#include "Network/Handler/NetworkHandler.h"
#include "Protocol/Packets/ChangeDimensionPacket.h"
#include "Protocol/Packets/DimensionDataPacket.h"
#include "World/Level/LevelGen/DimensionFactory.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "Protocol/Packets/SyncWorldClocksPacket.h"
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
    r.register = capture(() => event.blockComponentRegistry.registerCustomComponent("legacy:comp", {
        onPlayerInteract(...args) {
            r.legacyInteract = args.length + "|" + Object.getPrototypeOf(args[0]).constructor.name;
            world.setDynamicProperty("stable1", JSON.stringify(r));
        },
        onPlayerDestroy(event) {
            r.legacyDestroy = Object.keys(event).filter((k) => k !== "block" && k !== "dimension").sort().join("+");
            world.setDynamicProperty("stable1", JSON.stringify(r));
        },
        onPlayerBreak() {
            r.legacyBreak = "called";
        },
    }));
    r.legacyName = (() => {
        try {
            event.blockComponentRegistry.registerCustomComponent("nonamespace", {});
        } catch (e) {
            return e.name + "," + e.reason;
        }
        return "ok";
    })();
});
world.afterEvents.worldInitialize.subscribe(() => {
    world.setDynamicProperty("stable1", JSON.stringify(r));
});
)JS";

    const char *const COMPONENTS = R"JS(
import { world, system, BlockPermutation, CustomComponentParameters, BlockComponentPlayerInteractEvent,
         BlockEvent, ItemComponentUseOnEvent, ItemUseOnEvent, NamespaceNameError, WorldClock, Dimension } from "@minecraft/server";

const save = () => world.setDynamicProperty("components", JSON.stringify(r));
const describe = (event, params) => {
    const fields = Object.keys(event).filter((k) => k !== "block" && k !== "dimension").sort();
    return [Object.getPrototypeOf(event).constructor.name, fields.join("+"),
            params instanceof CustomComponentParameters ? JSON.stringify(params.params) : "none"].join("|");
};
const log = (name) => (event, params) => {
    r[name] = describe(event, params);
    save();
};

let placeCalls = 0;
system.beforeEvents.startup.subscribe((event) => {
    const blocks = event.blockComponentRegistry;
    const items = event.itemComponentRegistry;
    r.noNamespace = (() => {
        try {
            blocks.registerCustomComponent("nonamespace", {});
        } catch (e) {
            return (e instanceof NamespaceNameError) + "," + e.reason;
        }
        return "ok";
    })();
    r.minecraftNamespace = capture(() => items.registerCustomComponent("minecraft:thing", {}));
    r.notObject = capture(() => blocks.registerCustomComponent("test:bad", 3));
    r.registerBlock = capture(() => blocks.registerCustomComponent("test:hooks", {
        beforeOnPlayerPlace(event, params) {
            placeCalls++;
            r.beforePlace = describe(event, params) + "|" + event.permutationToPlace.type.id + "|" + event.face;
            if (placeCalls === 1)
                event.cancel = true;
            else
                event.permutationToPlace = BlockPermutation.resolve("minecraft:stone");
            save();
        },
        onPlace(event, params) {
            r.onPlace = describe(event, params) + "|" + event.previousBlock.type.id;
            save();
        },
        onBreak(event, params) {
            r.onBreak = describe(event, params) + "|" + (event.entitySource !== undefined);
            save();
        },
        onPlayerBreak: log("onPlayerBreak"),
        onPlayerInteract(event, params) {
            r.onPlayerInteract = describe(event, params) + "|" + event.face + "|" + event.faceLocation.x + ","
                + event.faceLocation.z + "|" + (event instanceof BlockComponentPlayerInteractEvent)
                + (event instanceof BlockEvent) + "|" + event.block.x;
            save();
        },
        onTick: log("onTick"),
        onRandomTick: log("onRandomTick"),
        onStepOn: log("onStepOn"),
        onStepOff: log("onStepOff"),
        onEntityFallOn(event, params) {
            r.onEntityFallOn = (r.onEntityFallOn || "") + describe(event, params) + "|" + event.fallDistance + ";";
            save();
        },
        onRedstoneUpdate(event, params) {
            r.onRedstoneUpdate = (r.onRedstoneUpdate || "") + event.previousPowerLevel + ">" + event.powerLevel + ";";
            save();
        },
        onPlayerDestroy: log("onPlayerDestroy"),
    }));
    r.duplicate = capture(() => blocks.registerCustomComponent("test:hooks", {}));
    r.registerItem = capture(() => items.registerCustomComponent("test:wand", {
        onUse: log("onUse"),
        onUseOn(event, params) {
            r.onUseOn = describe(event, params) + "|" + event.blockFace + "|" + event.faceLocation.y + "|"
                + (event instanceof ItemComponentUseOnEvent) + (event instanceof ItemUseOnEvent);
            save();
        },
        onCompleteUse: log("onCompleteUse"),
        onConsume: log("onConsume"),
        onHitEntity(event, params) {
            r.itemOrder = (r.itemOrder || "") + "hit;";
            log("onHitEntity")(event, params);
        },
        onBeforeDurabilityDamage(event, params) {
            r.itemOrder = (r.itemOrder || "") + "durability;";
            r.onBeforeDurabilityDamage = describe(event, params) + "|" + event.durabilityDamage;
            event.durabilityDamage = 7;
            save();
        },
        onMineBlock: log("onMineBlock"),
    }));
    r.itemDuplicate = capture(() => items.registerCustomComponent("test:wand", {}));
    r.litHook = capture(() => blocks.registerCustomComponent("test:lit_hook", {
        onTick(event, params) {
            r.litTick = JSON.stringify(params.params);
            save();
        },
    }));
    r.stateHook = capture(() => blocks.registerCustomComponent("test:states", {
        onBlockStateChange(event, params) {
            r.stateChange = Object.getPrototypeOf(event).constructor.name + "|"
                + event.previousPermutation.getState("test:lit") + "|" + event.previousPermutation.getState("test:level");
            save();
        },
    }));

    const dimensions = event.dimensionRegistry;
    dimensionsLate = dimensions;
    r.dimRegister = capture(() => dimensions.registerCustomDimension("test:void"));
    r.dimDuplicate = capture(() => dimensions.registerCustomDimension("test:void"));
    const reason = (name) => {
        try {
            dimensions.registerCustomDimension(name);
        } catch (e) {
            return e.name + "," + e.reason;
        }
        return "ok";
    };
    r.dimNoNamespace = reason("void");
    r.dimMinecraft = reason("minecraft:void");
    r.dimChars = capture(() => dimensions.registerCustomDimension("test:a b"));

    const clocks = event.worldClockRegistry;
    r.registerClock = capture(() => clocks.registerClock("test:clock",
        { timeMarkers: [{ name: "test:mark", time: 2, period: 5 }] }));
    r.clockDuplicate = capture(() => clocks.registerClock("test:clock"));
    r.clockName = capture(() => clocks.registerClock("noname"));
    r.clockNames = [" test:a", "test:a:b", "test", "minecraft:mine", "test:with space", "Test:Upper"]
        .map((name) => capture(() => clocks.registerClock(name)).startsWith("ok") ? "ok" : "no").join(",");
    r.clockPeriod = capture(() => clocks.registerClock("test:other",
        { timeMarkers: [{ name: "test:bad", time: 5, period: 5 }] }));
    save();
});

world.afterEvents.worldLoad.subscribe(() => {
    r.lateClock = capture(() => clocksLate.registerClock("test:late"));
    r.dimLate = capture(() => dimensionsLate.registerCustomDimension("test:late"));
    const custom = world.getDimension("test:void");
    r.dimId = custom.id;
    r.dimSame = custom === world.getDimension("test:void") && custom instanceof Dimension
        && custom !== world.getDimension("overworld");
    const clock = world.getClock("test:clock");
    r.clockSame = clock === world.getClock("test:clock") && clock instanceof WorldClock;
    r.clockMissing = capture(() => world.getClock("test:nope"));
    r.overworld = world.getClock("minecraft:overworld").timeMarkers
        .map((m) => m.name + "@" + m.time + "/" + m.period).join(",");
    r.resolve = BlockPermutation.resolve("test:stateful", { "test:level": 2 }).getState("test:level") + ","
        + BlockPermutation.resolve("test:stateful").getState("test:lit");
    world.afterEvents.worldClockOnTimeMarker.subscribe((event) => {
        r.markers = (r.markers || "") + event.clock.time + ";";
        save();
    }, { clock: "test:clock" });
    world.afterEvents.worldClockOnTimeMarker.subscribe((event) => {
        r.otherClock = event.clock.name;
    }, { clock: "test:none" });
    world.afterEvents.worldClockOnTimeModified.subscribe((event) => {
        r.modified = (r.modified || 0) + 1;
    }, { clock: "test:clock" });
    world.afterEvents.worldClockOnPaused.subscribe((event) => { r.paused = event.clock.name; }, { clock: "test:clock" });
    world.beforeEvents.worldClockOnRestart.subscribe((event) => {
        r.restart = event.clock.name + "|" + event.newTime;
        event.newTime = 5;
        save();
    }, { clock: "test:clock" });
    save();
});

let clocksLate;
let dimensionsLate;
system.beforeEvents.startup.subscribe((event) => { clocksLate = event.worldClockRegistry; });

system.runTimeout(() => {
    const clock = world.getClock("test:clock");
    r.clockTime = clock.time;
    clock.isPaused = true;
    r.rewind = capture(() => clock.rewindTo("test:mark")) + "@" + clock.time;
    r.skip = capture(() => clock.skipTo(clock.timeMarkers[0])) + "@" + clock.time;
    r.setMarker = capture(() => clock.set("test:mark")) + "@" + clock.time;
    clock.time = 1;
    r.rewindEarly = capture(() => clock.rewindTo("test:mark"));
    r.unknownMarker = capture(() => clock.set("test:unknown"));
    r.addMarker = capture(() => clock.addTimeMarker({ name: "test:added", time: 3 })) + ","
        + capture(() => clock.addTimeMarker({ name: "test:added", time: 4 }));
    const added = clock.timeMarkers.find((m) => m.name === "test:added");
    r.removeMarker = capture(() => clock.removeTimeMarker("test:added")) + "," + capture(() => added.time);
    r.removeMinecraft = capture(() => world.getClock("minecraft:overworld").removeTimeMarker("minecraft:noon"));
    const block = world.getDimension("overworld").getBlock({ x: 4, y: 70, z: 4 });
    r.setPermutation = capture(() => {
        block.setPermutation(BlockPermutation.resolve("test:stateful"));
        block.setPermutation(BlockPermutation.resolve("test:stateful", { "test:lit": true, "test:level": 1 }));
    });
    save();
}, 8);
)JS";

    const char *const COMPONENT_BLOCK = R"JSON({
  "format_version": "1.21.90",
  "minecraft:block": {
    "description": { "identifier": "test:block" },
    "components": {
      "minecraft:tick": { "interval_range": [10, 10] },
      "minecraft:redstone_consumer": { "min_power": 3 },
      "minecraft:entity_fall_on": { "min_fall_distance": 2 },
      "test:hooks": { "value": 42, "list": ["a", true] }
    }
  }
})JSON";

    const char *const COMPONENT_ITEM = R"JSON({
  "format_version": "1.21.90",
  "minecraft:item": {
    "description": { "identifier": "test:wand" },
    "components": { "test:wand": {} }
  }
})JSON";

    const char *const STATEFUL_BLOCK = R"JSON({
  "format_version": "1.21.90",
  "minecraft:block": {
    "description": {
      "identifier": "test:stateful",
      "states": { "test:lit": [false, true], "test:level": { "values": { "min": 0, "max": 2 } } }
    },
    "components": { "minecraft:tick": { "interval_range": [10, 10] }, "test:states": {} },
    "permutations": [
      { "condition": "q.block_state('test:lit') == true && q.block_state('test:level') >= 1",
        "components": { "test:lit_hook": { "lit": true }, "minecraft:geometry": "geometry.lamp",
                        "minecraft:material_instances": { "*": { "texture": "lit_tex", "render_method": "alpha_test" } } } }
    ]
  }
})JSON";

    const char *const LEGACY_BLOCK = R"JSON({
  "format_version": "1.21.40",
  "minecraft:block": {
    "description": { "identifier": "legacy:block" },
    "components": { "minecraft:custom_components": ["legacy:comp"] }
  }
})JSON";

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

    DimensionDefinition sentDimension;

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
        writePack(packs, "components", server2, std::string(PRELUDE) + COMPONENTS);
        writeFile(packs / "components" / "blocks" / "block.json", COMPONENT_BLOCK);
        writeFile(packs / "components" / "items" / "wand.json", COMPONENT_ITEM);
        writeFile(packs / "components" / "blocks" / "stateful.json", STATEFUL_BLOCK);
        writeFile(packs / "stable1" / "blocks" / "legacy.json", LEGACY_BLOCK);

        ServerNetworkHandler owner("Script API", "", 1);
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);

        BehaviorPackManager manager;
        manager.discover(packs.string());
        CustomContentRegistry::getInstance().load(manager, owner.getItemDefinitions(), owner.getBlockDefinitions());
        for (const BlockPropertyData &property: CustomContentRegistry::getInstance().getBlockProperties()) {
            if (property.mName != "test:stateful")
                continue;
            const Tag *permutations = property.mProperties.get("permutations");
            require(permutations != nullptr && permutations->getList().size() == 1, "permutations not sent");
            const Tag &first = permutations->getList().front();
            const Tag *components = first.get("components");
            require(components != nullptr && components->get("test:lit_hook") == nullptr,
                    "a custom component reached the client");
            const Tag *geometry = components->get("minecraft:geometry");
            const Tag *materials = components->get("minecraft:material_instances");
            require(geometry != nullptr && geometry->get("identifier")->asString() == "geometry.lamp",
                    "the permutation geometry was not sent");
            require(materials != nullptr
                            && materials->get("materials")->get("*")->get("texture")->asString() == "lit_tex"
                            && materials->get("materials")->get("*")->get("render_method")->asString() == "alpha_test",
                    "the permutation material was not sent");
            require(components->get("minecraft:friction") == nullptr, "an undeclared component was sent");
            const Tag *base = property.mProperties.get("components")->get("minecraft:material_instances");
            require(base->get("materials")->get("*")->get("texture")->asString() == "stateful",
                    "the permutation changed the base material");
            const Tag *states = property.mProperties.get("properties");
            require(states != nullptr && states->getList().size() == 2, "states not sent");
        }
        require(engine.loadPacks(manager.getPacks()) == 5, "only the five valid packs start");

        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 1001).first->second;
        engine.startup();

        bool customFound = false;
        const DimensionType voidDimension = Dimension::findCustom("test:void", customFound);
        require(customFound && Dimension::toId(voidDimension) == Dimension::FIRST_CUSTOM_ID
                        && Dimension::fromId(Dimension::FIRST_CUSTOM_ID) == voidDimension
                        && std::string(Dimension::getName(voidDimension)) == "test:void",
                "the custom dimension did not get the first custom id");
        World &defaultWorld = owner.getWorlds().getDefault();

        // a packet sensor: every encoded packet, dropped before any transport
        require(owner.isTransportReady(), "the test server has no network handler to observe");
        std::vector<std::pair<MinecraftPacketIds, std::string>> sent;
        owner.getNetworkHandler().setOutboundFilter([&sent](const NetworkIdentifier &, std::string &data) {
            ReadOnlyBinaryStream header(data);
            unsigned char sender = 0, client = 0;
            sent.emplace_back(Packet::peekId(header, sender, client), data);
            return false;
        });
        LoginHandler::sendStartGame(owner, player);
        size_t startGameAt = sent.size(), dimensionsAt = sent.size();
        for (size_t index = 0; index < sent.size(); ++index) {
            if (sent[index].first == MinecraftPacketIds::StartGame && startGameAt == sent.size())
                startGameAt = index;
            if (sent[index].first == MinecraftPacketIds::DimensionData && dimensionsAt == sent.size())
                dimensionsAt = index;
        }
        require(startGameAt < sent.size() && dimensionsAt == startGameAt + 1,
                "DimensionDataPacket does not follow StartGame");
        ReadOnlyBinaryStream body(sent[dimensionsAt].second);
        DimensionDataPacket dimensionData;
        dimensionData.readHeader(body);
        dimensionData.read(body, owner.getCodecContext());
        require(dimensionData.mDefinitions.size() == 1 && dimensionData.mDefinitions[0].mId == "test:void",
                "DimensionDataPacket does not carry the custom dimension");
        sentDimension = dimensionData.mDefinitions[0];
        require(sentDimension.mMinimumHeight == -64 && sentDimension.mMaximumHeight == 384
                        && sentDimension.mGeneratorType == 5 && sentDimension.mCloudHeight == 128
                        && !sentDimension.mRenderClouds,
                "DimensionDataPacket does not carry the game's script dimension definition");

        sent.clear();
        owner.changePlayerLevel(player, defaultWorld.getLevel(voidDimension), Vector3f(0.5f, 80.0f, 0.5f));
        require(player.getDimension() == voidDimension, "the player did not move to the custom dimension");
        bool changed = false;
        for (const auto &packet: sent) {
            if (packet.first != MinecraftPacketIds::ChangeDimension)
                continue;
            ReadOnlyBinaryStream change(packet.second);
            ChangeDimensionPacket decoded;
            decoded.readHeader(change);
            decoded.read(change, owner.getCodecContext());
            changed = decoded.mDimension == Dimension::FIRST_CUSTOM_ID;
        }
        require(changed, "no ChangeDimensionPacket named the custom dimension");
        owner.changePlayerLevel(player, defaultWorld.getOverworld(), Vector3f(0.5f, 80.0f, 0.5f));
        owner.getNetworkHandler().setOutboundFilter(nullptr);
        require(defaultWorld.hasLevel(voidDimension)
                        && defaultWorld.getLevel(voidDimension).getDimensionType() == voidDimension
                        && &defaultWorld.getLevel(voidDimension) != &defaultWorld.getOverworld(),
                "no level was created for the custom dimension");
        const std::unique_ptr<ChunkGenerator> generator = DimensionFactory::createGenerator(voidDimension, 1);
        const std::unique_ptr<LevelChunk> terrain = generator->generateTerrain(0, 0);
        require(terrain->getBlock(0, 64, 0).mName == "minecraft:air"
                        && terrain->getBlock(0, LevelChunk::MIN_Y, 0).mName == "minecraft:air",
                "the custom dimension is not generated empty");

        Level &level = owner.getLevelFor(player);
        const Vector3i at(4, 70, 4);
        BlockState placed("test:block");
        require(engine.beforeCustomBlockPlace(player, level, at, 1, placed), "beforeOnPlayerPlace did not cancel");
        require(!engine.beforeCustomBlockPlace(player, level, at, 1, placed) && placed.mName == "minecraft:stone",
                "beforeOnPlayerPlace did not replace the permutation");
        engine.onCustomBlockPlaced(level, at, BlockState("test:block"), BlockState("minecraft:air"));
        engine.onPlayerInteractWithBlock(player, "test:block", at, 1, Vector3f(0.25f, 1.0f, 0.75f));
        engine.onPlayerInteractWithBlock(player, "legacy:block", at, 1, Vector3f(0.25f, 1.0f, 0.75f));
        engine.onCustomBlockTick(level, "test:block", at);
        engine.onCustomBlockRandomTick(level, "test:block", at);
        engine.onCustomBlockStep(player, level, "test:block", at);
        engine.onCustomBlockStepOff(player, level);
        engine.onCustomBlockFallOn(player, level, "test:block", at, 1.5f);
        engine.onCustomBlockFallOn(player, level, "test:block", at, 3.5f);
        for (int32_t power: {0, 2, 5, 5, 9, 0})
            engine.onCustomBlockRedstone(level, "test:block", at, power);
        engine.onCustomBlockBreak(level, at, BlockState("test:block"), &player);
        PlayerBreakBlockAfterEvent legacyBroken(player, at, "legacy:block");
        owner.getEventBus().after().mPlayerBreakBlock.emit(legacyBroken);

        ItemStack wand;
        wand.mDefinition = owner.getItemDefinitions().getDefinition("test:wand");
        wand.mCount = 1;
        require(wand.mDefinition != nullptr, "the custom item was not registered");
        player.getInventory().setItemInHand(wand);
        engine.onItemUse(player);
        engine.onItemUseOnBlock(player, at, 1, Vector3f(0.5f, 1.0f, 0.5f));
        engine.onItemCompleteUse(player, wand);
        require(engine.onItemHitEntity(player, player, true, 2) == 7, "onBeforeDurabilityDamage was not applied");
        PlayerBreakBlockAfterEvent broken(player, at, "test:block");
        owner.getEventBus().after().mPlayerBreakBlock.emit(broken);
        player.getInventory().setItemInHand(ItemStack::air());

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

        const BlockState stateful = level.getBlockState(at.x, at.y, at.z);
        require(stateful.mName == "test:stateful" && stateful.mStates.get("test:lit") != nullptr
                        && stateful.mStates.get("test:lit")->asByte() == 1
                        && stateful.mStates.get("test:level")->asInt() == 1,
                "setPermutation did not keep the states");
        engine.onCustomBlockTick(level, "test:stateful", at);

        WorldClock *clock = engine.worldClocks().find("test:clock");
        require(clock != nullptr, "the script clock was not registered");
        engine.worldClocks().setPaused(*clock, false);
        engine.worldClocks().setTime(*clock, INT32_MAX);
        engine.tick(9);
        require(clock->mTime == 5, "worldClockOnRestart did not set the new time");

        engine.worldClocks().setPaused(*clock, true);
        const Tag saved = engine.worldClocks().save();
        const Tag *found = nullptr;
        for (const Tag &entry: saved.get("clocks")->getList()) {
            if (entry.get("Name")->asString() == "test:clock")
                found = &entry;
        }
        require(found != nullptr, "the script clock was not saved");
        const Tag &entries = *found;
        require(entries.get("Name")->asString() == "test:clock" && entries.get("Time")->asInt() == 5
                        && entries.get("IsPaused")->asByte() == 1,
                "the clocks were not saved in the game's form");
        WorldClockSystem restored;
        restored.load(saved);
        bool markerError = false;
        require(restored.registerClock("test:clock", {}, markerError) == WorldClockError::None
                        && restored.find("test:clock")->mTime == 5 && restored.find("test:clock")->mPaused,
                "a registered clock did not take its saved state");
        int64_t levelTime = 1000;
        bool levelPaused = false;
        WorldClockSystem::LevelTime binding;
        binding.mGet = [&levelTime]() { return levelTime; };
        binding.mSet = [&levelTime](int64_t value) { levelTime = value; };
        binding.mSetPaused = [&levelPaused](bool paused) { levelPaused = paused; };
        restored.bindOverworld(binding);
        WorldClock *overworldClock = restored.find("minecraft:overworld");
        require(overworldClock->mTime == 1000, "the overworld clock does not start at the level's time");
        levelTime = 1001;
        restored.tick();
        require(overworldClock->mTime == 1001, "the overworld clock does not follow the level's time");
        restored.setTime(*overworldClock, 6000);
        require(levelTime == 6000, "setting the overworld clock does not set the level's time");
        restored.setPaused(*overworldClock, true);
        require(levelPaused, "pausing the overworld clock does not stop the level's day");
        restored.setPaused(*overworldClock, false);
        SyncWorldClocksPacket states;
        restored.writeStates(states);
        require(states.mType == SyncWorldClocksType::SyncState && states.mClockStates.size() == 2
                        && states.mClockStates.front().mClockId == 0x63D7EDE73C38F916ull,
                "the periodic sync does not carry every clock under its hashed id");

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

        const std::unique_ptr<json::Value> components = result(owner, "components");
        const json::Value &c = *components;
        const std::string params = "{\"value\":42,\"list\":[\"a\",true]}";
        expect(c, "noNamespace", "true,NoNamespace");
        expect(c, "minecraftNamespace", "NamespaceNameError: ");
        expect(c, "notObject", "TypeError: Native type conversion failed.");
        expect(c, "registerBlock", "ok");
        expect(c, "duplicate",
               "BlockCustomComponentAlreadyRegisteredError: Block custom component 'test:hooks' was already registered.");
        expect(c, "registerItem", "ok");
        expect(c, "itemDuplicate",
               "ItemCustomComponentAlreadyRegisteredError: Item custom component 'test:wand' was already registered.");
        expect(c, "beforePlace",
               "BlockComponentPlayerPlaceBeforeEvent|cancel+face+permutationToPlace+player|" + params
                       + "|test:block|Up");
        expect(c, "onPlace", "BlockComponentOnPlaceEvent|previousBlock|" + params + "|minecraft:air");
        expect(c, "onPlayerInteract",
               "BlockComponentPlayerInteractEvent|face+faceLocation+player|" + params + "|Up|0.25,0.75|truetrue|4");
        expect(c, "onTick", "BlockComponentTickEvent||" + params);
        expect(c, "onRandomTick", "BlockComponentRandomTickEvent||" + params);
        expect(c, "onStepOn", "BlockComponentStepOnEvent|entity|" + params);
        expect(c, "onStepOff", "BlockComponentStepOffEvent|entity|" + params);
        expect(c, "onEntityFallOn", "BlockComponentEntityFallOnEvent|entity+fallDistance|" + params + "|3.5;");
        expect(c, "onRedstoneUpdate", "2>5;5>9;");
        expect(c, "onBreak", "BlockComponentBlockBreakEvent|brokenBlockPermutation+entitySource|" + params + "|true");
        expect(c, "onPlayerBreak", "BlockComponentPlayerBreakEvent|brokenBlockPermutation+player|" + params);
        require(c.get("onPlayerDestroy") == nullptr, "a 2.x pack received the 1.x onPlayerDestroy hook");
        expect(c, "onUse", "ItemComponentUseEvent|itemStack+source|{}");
        expect(c, "onUseOn",
               "ItemComponentUseOnEvent|blockFace+faceLocation+itemStack+source+usedOnBlockPermutation|{}|Up|1|"
               "truetrue");
        expect(c, "onCompleteUse", "ItemComponentCompleteUseEvent|itemStack+source|{}");
        expect(c, "onConsume", "ItemComponentConsumeEvent|itemStack+source|{}");
        expect(c, "onHitEntity", "ItemComponentHitEntityEvent|attackingEntity+hadEffect+hitEntity+itemStack|{}");
        expect(c, "onBeforeDurabilityDamage",
               "ItemComponentBeforeDurabilityDamageEvent|attackingEntity+durabilityDamage+hitEntity+itemStack|{}|2");
        expect(c, "onMineBlock", "ItemComponentMineBlockEvent|itemStack+minedBlockPermutation+source|{}");
        expect(c, "litHook", "ok");
        expect(c, "stateHook", "ok");
        expect(c, "resolve", "2,false");
        expect(c, "setPermutation", "ok");
        expect(c, "stateChange", "BlockComponentBlockStateChangeEvent|false|0");
        expect(c, "litTick", "{\"lit\":true}");
        expect(c, "registerClock", "ok");
        expect(c, "dimRegister", "ok");
        expect(c, "dimDuplicate",
               "CustomDimensionAlreadyRegisteredError: Custom dimension 'test:void' was already registered.");
        expect(c, "dimNoNamespace", "NamespaceNameError,NoNamespace");
        expect(c, "dimMinecraft", "NamespaceNameError,DisallowedNamespace");
        expect(c, "dimChars", "CustomDimensionNameError: Custom dimension 'test:a b' has invalid characters.");
        expect(c, "dimLate",
               "CustomDimensionInvalidRegistryError: Cannot register custom dimension 'test:late' outside of "
               "StartupEvent.");
        expect(c, "dimId", "test:void");
        expect(c, "dimSame", "true");
        expect(c, "itemOrder", "durability;hit;");
        expect(c, "clockNames", "no,no,no,no,no,ok");
        expect(c, "clockDuplicate",
               "WorldClockRegistrationError: Failed to register world clock 'test:clock'. World clock name already "
               "exists.");
        expect(c, "clockName",
               "WorldClockRegistrationError: Failed to register world clock 'noname'. World clock has invalid or "
               "missing name.");
        expect(c, "clockPeriod",
               "WorldClockRegistrationError: Failed to register world clock 'test:other'. Time marker period must be "
               "greater than its time.");
        expect(c, "lateClock",
               "WorldClockInvalidRegistryError: Cannot register world clock 'test:late' outside of StartupEvent.");
        expect(c, "clockSame", "true");
        expect(c, "clockMissing", "WorldClockNotFoundError: Failed to find world clock 'test:nope'.");
        expect(c, "overworld",
               "minecraft:day@1000/24000,minecraft:noon@6000/24000,minecraft:sunset@12000/24000,"
               "minecraft:night@13000/24000,minecraft:midnight@18000/24000,minecraft:sunrise@23000/24000");
        expect(c, "clockTime", "8");
        expect(c, "rewind", "ok@7");
        expect(c, "skip", "ok@7");
        expect(c, "setMarker", "ok@2");
        expect(c, "rewindEarly",
               "WorldClockRewindError: Failed to rewind world clock 'test:clock'. Current time is behind time marker "
               "'test:mark'");
        expect(c, "unknownMarker",
               "WorldClockTimeMarkerNotFoundError: Failed to find time marker 'test:unknown' in world clock "
               "'test:clock'.");
        expect(c, "addMarker",
               "ok,WorldClockAddTimeMarkerError: Failed to add time marker 'test:added' on world clock 'test:clock' "
               "Time marker name already exists.");
        expect(c, "removeMarker", "ok,WorldClockInvalidTimeMarkerError: ");
        expect(c, "removeMinecraft",
               "WorldClockRemoveMinecraftTimeMarkerError: Failed to remove time marker 'minecraft:noon' from world "
               "clock 'minecraft:overworld'. A time marker with the 'minecraft' namespace cannot be removed.");
        expect(c, "paused", "test:clock");
        expect(c, "markers", "2;7;7;2;2147483647;");
        expect(c, "modified", "4");
        expect(c, "restart", "test:clock|0");
        require(c.get("otherClock") == nullptr, "a clock filter let another clock's event through");

        const std::unique_ptr<json::Value> stable1 = result(owner, "stable1");
        expect(*stable1, "alone", "true");
        expect(*stable1, "gameMode", "survival,undefined");
        expect(*stable1, "worldInitialize", "true");
        expect(*stable1, "worldLoad", "false");
        expect(*stable1, "systemBefore", "false");
        expect(*stable1, "early", "ok");
        expect(*stable1, "startupEvent", "false");
        expect(*stable1, "register", "ok");
        expect(*stable1, "legacyName", "CustomComponentNameError,1");
        expect(*stable1, "legacyInteract", "1|BlockComponentPlayerInteractEvent");
        expect(*stable1, "legacyDestroy", "destroyedBlockPermutation+player");
        require(stable1->get("legacyBreak") == nullptr, "a 1.x pack received the 2.x onPlayerBreak hook");

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
    CustomContentRegistry::getInstance().reset();
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
