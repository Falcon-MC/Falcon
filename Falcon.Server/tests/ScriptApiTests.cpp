#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/ScriptEngine.h"
#include "Server/ServerPaths.h"
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
                   const std::string &script) {
        static int serial = 0;
        ++serial;
        const std::string header = "00000000-0000-4000-8000-" + std::to_string(100000000000 + serial);
        const std::string module = "00000000-0000-4000-9000-" + std::to_string(100000000000 + serial);
        writeFile(root / name / "manifest.json",
                  "{\"format_version\": 2,\n"
                  " \"header\": {\"name\": \"" + name + "\", \"uuid\": \"" + header + "\", \"version\": [1, 0, 0],"
                  " \"min_engine_version\": [1, 21, 0]},\n"
                  " // comments are allowed in manifests\n"
                  " \"modules\": [{\"type\": \"script\", \"language\": \"javascript\", \"uuid\": \"" + module + "\","
                  " \"entry\": \"scripts/main.js\", \"version\": [1, 0, 0]}],\n"
                  " \"dependencies\": [" + dependencies + "]}\n");
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
         ScreenDisplay, TickingAreaManager } from "@minecraft/server";
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
        writePack(packs, "beta", "{\"module_name\": \"@minecraft/server\", \"version\": \"beta\"},"
                                 " {\"module_name\": \"@minecraft/server-ui\", \"version\": \"2.4.0-beta\"}",
                  std::string(PRELUDE) + BETA);
        writePack(packs, "undeclared", server2, UNDECLARED);
        writePack(packs, "net", server2 + ", {\"module_name\": \"@minecraft/server-net\", \"version\": \"1.0.0-beta\"}",
                  NOT_IMPLEMENTED);
        writePack(packs, "badversion", "{\"module_name\": \"@minecraft/server\", \"version\": \"2.99.0\"}", NEVER_RUNS);
        writePack(packs, "unknown", server2 + ", {\"module_name\": \"@minecraft/server-unknown\", \"version\": \"1.0.0\"}",
                  NEVER_RUNS);
        writeFile(packs / "outside.js", "export const x = 1;\n");

        ServerNetworkHandler owner("Script API", "", 1);
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);

        BehaviorPackManager manager;
        manager.discover(packs.string());
        require(engine.loadPacks(manager.getPacks()) == 3, "only the three valid packs start");

        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 1001).first->second;
        engine.startup();

        MobEffectInstance effect;
        effect.mId = MobEffectId::Speed;
        effect.mDuration = 20;
        engine.beforeEffectAdd(player, effect);
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
        expect(*stable2, "prototypeBound",
               "ReferenceError: Native function [Entity::addTag] object bound to prototype does not exist.");
        expect(*stable2, "startupEarly",
               "ReferenceError: Native function [World::getAllPlayers] cannot be used in early execution.");
        expect(*stable2, "register", "ok");
        expect(*stable2, "lateRegister", "CustomComponentInvalidRegistryError: Cannot register custom component "
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
        expect(*stable2, "invalid", "InvalidEntityError: Failed to get property 'location' due to Entity being "
                                    "invalid (has the Entity been removed?).");
        expect(*stable2, "invalidClass", "true");
        expect(*stable2, "isValid", "false");

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
            / ("falcon-scripts-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
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
