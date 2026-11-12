#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "Scripting/ScriptPackConfiguration.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Player/ServerPlayer.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
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

    void writePack(const std::filesystem::path &root, const std::string &name, const std::string &moduleUuid,
                   const std::string &dependencies, const std::string &script) {
        static int serial = 0;
        ++serial;
        const std::string header = "00000000-0000-4000-8000-" + std::to_string(200000000000 + serial);
        writeFile(root / name / "manifest.json",
                  "{\"format_version\": 2, \"header\": {\"name\": \"" + name + "\", \"uuid\": \"" + header
                          + "\", \"version\": [1, 0, 0], \"min_engine_version\": [1, 21, 0]},"
                            " \"modules\": [{\"type\": \"script\", \"language\": \"javascript\", \"uuid\": \""
                          + moduleUuid
                          + "\", \"entry\": \"scripts/main.js\", \"version\": [1, 0, 0]}],"
                            " \"dependencies\": ["
                          + dependencies + "]}\n");
        writeFile(root / name / "scripts" / "main.js", script);
    }

    const char *const SERVER_BETA = "{\"module_name\": \"@minecraft/server\", \"version\": \"beta\"}";
    const char *const ADMIN_BETA = "{\"module_name\": \"@minecraft/server-admin\", \"version\": \"1.0.0-beta\"}";
    const char *const ADMIN_UUID = "0000000a-0000-4000-9000-000000000001";
    const char *const REFUSER_UUID = "0000000a-0000-4000-9000-000000000002";

    const char *const ADMIN = R"JS(
import { world, system } from "@minecraft/server";
import * as admin from "@minecraft/server-admin";
const r = {};
const capture = (f) => {
    try {
        const value = f();
        return value === undefined ? "ok" : String(value);
    } catch (e) {
        return e.name + ": " + e.message;
    }
};
// early execution: variables and secrets are allowed, the rest is not
r.earlyNames = capture(() => admin.variables.names);
r.level = capture(() => admin.variables.get("level"));
r.nested = capture(() => JSON.stringify(admin.variables.get("nested")));
r.missing = capture(() => admin.variables.get("nope"));
const secret = admin.secrets.get("token");
r.secret = secret instanceof admin.SecretString && JSON.stringify(secret) === "{}" && !String(secret).includes("hunter2");
r.noSecret = admin.secrets.get("nope") === undefined;
r.ownSecret = capture(() => new admin.SecretString("x") instanceof admin.SecretString);
r.earlyKick = capture(() => admin.kickPlayer(undefined));
r.serverConstructor = capture(() => new admin.ServerVariables());
r.dedicated = admin.dedicatedServer instanceof admin.DedicatedServerUtils;
r.earlySubscribe = capture(() => admin.beforeEvents.asyncPlayerJoin.subscribe(async () => {}));
system.afterEvents.scriptEventReceive.subscribe((event) => {
    if (event.id !== "admin:host")
        return;
    const host = world.getAllPlayers()[0];
    r.badNetherNet = capture(() => admin.transferPlayer(host, { netherNetId: "12ab" }));
    try {
        admin.transferPlayer(host, { netherNetId: "x" });
    } catch (e) {
        r.badNetherNetKind = e.type + ":" + e.index;
    }
    r.netherNet = capture(() => admin.transferPlayer(host, { netherNetId: "00042" }));
    r.netherNetUuid = capture(() => admin.transferPlayer(host, { netherNetId: "0123456789ABCDEF0123456789ABCDEF" }));
    r.left = host;
    world.setDynamicProperty("admin", JSON.stringify(r));
});
system.afterEvents.scriptEventReceive.subscribe((event) => {
    if (event.id !== "admin:left")
        return;
    r.leftAdd = capture(() => admin.dedicatedServer.allowList.add(r.left));
    r.leftContains = capture(() => admin.dedicatedServer.allowList.contains(r.left));
    delete r.left;
    world.setDynamicProperty("admin", JSON.stringify(r));
});
world.afterEvents.worldLoad.subscribe(() => {
    r.variableNames = admin.variables.names.join(",");
    r.secretNames = admin.secrets.names.join(",");
    admin.beforeEvents.asyncPlayerJoin.subscribe((event) => {
        if (event.name !== "Guest")
            return new Promise((resolve) => system.run(() => {
                r.lateAllow = capture(() => event.allowJoin());
                r.lateValid = event.isValid();
                world.setDynamicProperty("admin", JSON.stringify(r));
                resolve();
            }));
        r.joinName = event.name;
        r.joinValid = event.isValid();
        r.persistentId = event.persistentId;
        r.split = ("splitScreenSlot" in event) + "," + event.splitScreenSlot;
        return new Promise((resolve) => system.run(() => {
            r.allow = capture(() => event.allowJoin());
            world.setDynamicProperty("admin", JSON.stringify(r));
            resolve();
        }));
    });
    const list = admin.dedicatedServer.allowList;
    r.add = capture(() => list.add({ name: "Steve" }, true));
    r.duplicate = capture(() => list.add({ name: "steve" }));
    r.contains = list.contains({ name: "Steve" }) + "," + list.contains({ name: "Alex" });
    r.entries = JSON.stringify(list.entries);
    r.remove = capture(() => list.remove({ name: "Steve" }));
    r.removeAgain = capture(() => list.remove({ name: "Steve" }));
    r.enabled = list.enabled;
    r.badDetails = capture(() => list.contains(5));
    r.emptyEntry = capture(() => list.add({}));
    try {
        list.contains({ name: "", xuid: "" });
    } catch (e) {
        r.emptyEntryKind = e.type + ":" + e.index;
    }
    r.reload = capture(() => list.reloadFile());
    const storage = admin.dedicatedServer.levelStorage;
    storage.saveResume().then(() => { r.resumeFirst = "ok"; }, (e) => { r.resumeFirst = e.name + ": " + e.message; });
    storage.saveHold().then(() => storage.saveHold()).then(() => { r.holdTwice = "ok"; },
        (e) => { r.holdTwice = e.name; return storage.saveQuery(); })
        .then((files) => {
            r.query = Array.isArray(files) && files.every((f) => f instanceof admin.LevelStorageQuerySnapshotFile);
            return storage.saveResume();
        })
        .then(() => {
            r.resumed = "ok";
            world.setDynamicProperty("admin", JSON.stringify(r));
        });
});
)JS";

    const char *const REFUSER = R"JS(
import { world } from "@minecraft/server";
import { beforeEvents } from "@minecraft/server-admin";
world.afterEvents.worldLoad.subscribe(() => beforeEvents.asyncPlayerJoin.subscribe(async (event) => {
    if (event.name === "Intruder")
        event.disallowJoin("go away");
}));
)JS";

    const char *const BLOCKED = R"JS(
import { world } from "@minecraft/server";
world.afterEvents.worldLoad.subscribe(() => world.setDynamicProperty("blocked", "ran"));
)JS";

    std::unique_ptr<json::Value> result(ServerNetworkHandler &owner) {
        const auto it = owner.getWorldDynamicProperties().find("admin");
        require(it != owner.getWorldDynamicProperties().end(), "no result from the admin pack");
        std::unique_ptr<json::Value> parsed = json::parse(it->second.mString);
        require(parsed != nullptr && parsed->isObject(), "the admin pack wrote no JSON");
        return parsed;
    }

    void expect(const json::Value &values, const char *key, const std::string &wanted) {
        const json::Value *value = values.get(key);
        require(value != nullptr, std::string("missing result ") + key);
        std::string actual = value->string();
        if (value->mType == json::Value::Type::Boolean)
            actual = value->mBoolean ? "true" : "false";
        require(actual == wanted, std::string(key) + ": expected \"" + wanted + "\", got \"" + actual + "\"");
    }

    void run(const std::filesystem::path &directory) {
        const std::filesystem::path packs = directory / "packs";
        writePack(packs, "admin", ADMIN_UUID, std::string(SERVER_BETA) + ", " + ADMIN_BETA, ADMIN);
        writePack(packs, "refuser", REFUSER_UUID, std::string(SERVER_BETA) + ", " + ADMIN_BETA, REFUSER);
        writePack(packs, "blocked", "0000000a-0000-4000-9000-000000000003",
                  std::string(SERVER_BETA)
                          + ", {\"module_name\": \"@minecraft/server-graphics\", \"version\": "
                            "\"1.0.0\"}",
                  BLOCKED);
        writeFile(directory / "config" / "default" / "permissions.json",
                  R"({"allowed_modules": ["@minecraft/server", "@minecraft/server-admin"]})");
        writeFile(directory / "config" / ADMIN_UUID / "permissions.json", R"({"allowed_modules": ["*"]})");
        writeFile(directory / "config" / ADMIN_UUID / "variables.json", R"({"level": 22, "nested": {"a": [1, "b"]}})");
        writeFile(directory / "config" / ADMIN_UUID / "secrets.json", R"({"token": "hunter2"})");

        ServerNetworkHandler owner("Script admin", "", 2);
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);
        BehaviorPackManager manager;
        manager.discover(packs.string());
        require(engine.loadPacks(manager.getPacks()) == 2,
                "the pack depending on a module the default permissions leave out started");

        NetworkIdentifier guest;
        ServerPlayer &player = owner.getPlayers().try_emplace(guest, guest, 2001).first->second;
        player.setName("Guest");
        player.setPlayFabId("pf-guest");
        engine.startup();
        engine.tick(0);

        bool joined = false;
        require(engine.holdPlayerJoin(player, [&joined]() { joined = true; }), "asyncPlayerJoin did not hold");
        require(engine.isHoldingJoin(guest) && !joined, "the join went on before the promise settled");
        engine.tick(1);
        engine.tick(2);
        require(joined && !engine.isHoldingJoin(guest), "the join did not go on once every promise settled");

        NetworkIdentifier intruderId(std::string("10.0.0.9"), 19132);
        ServerPlayer &intruder = owner.getPlayers().try_emplace(intruderId, intruderId, 2002).first->second;
        intruder.setName("Intruder");
        bool intruderJoined = false;
        engine.holdPlayerJoin(intruder, [&intruderJoined]() { intruderJoined = true; });
        engine.tick(3);
        engine.tick(4);
        require(!intruderJoined && owner.getPlayers().find(intruderId) == owner.getPlayers().end(),
                "disallowJoin let the player in");

        // a paused world lets the join through without the scripts
        owner.tick(true);
        NetworkIdentifier pausedId(std::string("10.0.0.10"), 19132);
        ServerPlayer &paused = owner.getPlayers().try_emplace(pausedId, pausedId, 2003).first->second;
        paused.setName("Paused");
        require(!engine.holdPlayerJoin(paused, []() {}), "a paused world held the join");
        owner.tick(false);

        NetworkIdentifier hostId(std::string("10.0.0.11"), 19132);
        ServerPlayer &host = owner.getPlayers().try_emplace(hostId, hostId, 2004).first->second;
        host.setName("Host");
        host.setLoginState(ServerPlayer::LoginState::Spawned);
        engine.emitScriptEvent("admin:host", "");
        owner.erasePlayer(hostId);
        engine.emitScriptEvent("admin:left", "");

        const std::unique_ptr<json::Value> values = result(owner);
        require(owner.getWorldDynamicProperties().count("blocked") == 0, "the blocked pack ran");
        expect(*values, "variableNames", "level,nested");
        expect(*values, "earlyNames",
               "ReferenceError: Native property getter [ServerVariables::names] cannot be used in early execution.");
        expect(*values, "level", "22");
        expect(*values, "nested", R"({"a":[1,"b"]})");
        expect(*values, "missing", "ok");
        expect(*values, "secretNames", "token");
        expect(*values, "secret", "true");
        expect(*values, "noSecret", "true");
        expect(*values, "ownSecret", "true");
        expect(*values, "earlyKick", "ReferenceError: Native function [kickPlayer] cannot be used in early execution.");
        expect(*values, "serverConstructor", "ReferenceError: No constructor for native class 'ServerVariables'.");
        expect(*values, "dedicated", "true");
        expect(*values, "earlySubscribe",
               "ReferenceError: Native property getter [AdminBeforeEvents::asyncPlayerJoin] cannot be used in early "
               "execution.");
        expect(*values, "joinName", "Guest");
        expect(*values, "joinValid", "true");
        expect(*values, "persistentId", "pf-guest");
        expect(*values, "split", "true,undefined");
        expect(*values, "emptyEntry", "InvalidArgumentError: Entry data must contain a name and/or a xuid");
        expect(*values, "emptyEntryKind", "Empty:0");
        expect(*values, "badNetherNet",
               "InvalidArgumentError: NetherNet Id provided '12ab' does not match expected format.");
        expect(*values, "badNetherNetKind", "Unknown:1");
        expect(*values, "netherNet", "ok");
        expect(*values, "netherNetUuid", "ok");
        expect(*values, "leftAdd",
               "InvalidEntityError: Failed to call 'add' due to Entity being invalid (has the Entity been removed?).");
        expect(*values, "leftContains",
               "InvalidEntityError: Failed to call 'contains' due to Entity being invalid (has the Entity been "
               "removed?).");
        expect(*values, "allow", "ok");
        expect(*values, "lateAllow",
               "DisconnectedError: Tried to disconnect client after client was already disconnected.");
        expect(*values, "lateValid", "false");
        expect(*values, "add", "ok");
        expect(*values, "duplicate", "AllowListModificationError: Unable to add player 'steve' to allow list");
        expect(*values, "contains", "true,false");
        expect(*values, "entries", R"([{"ignoresPlayerLimit":true,"name":"Steve"}])");
        expect(*values, "remove", "ok");
        expect(*values, "removeAgain", "AllowListModificationError: Unable to remove player 'Steve' from allow list");
        expect(*values, "enabled", "false");
        expect(*values, "badDetails", "TypeError: Native type conversion failed.");
        expect(*values, "reload", "ok");
        expect(*values, "resumeFirst",
               "LevelStorageSaveStateChangeError: Level storage may be busy with previous commands.");
        expect(*values, "holdTwice", "LevelStorageSaveStateChangeError");
        expect(*values, "query", "true");
        expect(*values, "resumed", "ok");
        require(owner.isAutoSaveEnabled(), "save resume did not turn saving back on");

        // a config folder without default/permissions.json gets the dedicated server's default list
        const std::filesystem::path fresh = directory / "fresh-config";
        ScriptPackConfigurationManager configs;
        configs.load(fresh.string());
        std::ifstream created(fresh / "default" / "permissions.json");
        std::stringstream text;
        text << created.rdbuf();
        require(text.str().find("\"@minecraft/server-editor\"") != std::string::npos,
                "the default permissions.json was not written");
        require(!configs.get("anything").allowsModule("@minecraft/server-net")
                        && configs.get("anything").allowsModule("@minecraft/server-admin"),
                "the default permissions.json does not hold the default module list");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-script-admin-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Script admin regression failed: " << error.what() << '\n';
        result = 1;
    }
    CustomContentRegistry::getInstance().reset();
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
