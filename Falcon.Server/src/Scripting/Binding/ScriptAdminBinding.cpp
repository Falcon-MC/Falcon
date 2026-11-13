#include "Scripting/Binding/ScriptAdminBinding.h"

#include "Core/Debug/BedrockLog.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/TransferPacket.h"
#include "Scripting/Binding/ScriptApi.h"
#include "Scripting/ScriptPackConfiguration.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Player/ServerPlayer.h"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_set>

namespace {
    // the module's natives go in, the official classes and globals come out; secrets stay on the native side
    const char *const ADMIN = R"JS(
(function (native) {
    const token = Symbol();
    const errors = native.errors;
    const conversion = () => new TypeError("Native type conversion failed.");
    const guard = (key, name) => {
        if (key !== token)
            throw new ReferenceError("No constructor for native class '" + name + "'.");
    };
    // the privilege tags of the definitions: early-execution-allowed and no-restricted-execution
    const check = (label, member) => {
        const [early, blocked] = native.flags(member);
        const privilege = native.privilege();
        if (privilege === 1 && !early)
            throw new ReferenceError(label + " [" + member.replace(".", "::") + "] cannot be used in early execution.");
        if (privilege === 2 && blocked)
            throw new ReferenceError(label + " [" + member.replace(".", "::") + "] cannot be used in restricted execution.");
    };
    const fn = (member) => check("Native function", member);
    const define = (cls, name) => {
        Object.defineProperty(cls, "name", { value: name });
        return cls;
    };
    const method = (cls, name, body) => Object.defineProperty(cls.prototype, name, {
        value: body, writable: true, configurable: true });
    const getter = (cls, name, get, set) => Object.defineProperty(cls.prototype, name, {
        get, set, configurable: true });

    // a SecretString is a name of secrets.json or a script's own value; neither is readable back
    const secretOf = new WeakMap();
    const SecretString = define(class {
        constructor(value) {
            if (arguments.length < 1 || typeof value !== "string")
                throw conversion();
            secretOf.set(this, { value });
        }
    }, "SecretString");
    const makeSecret = (name) => {
        const secret = Object.create(SecretString.prototype);
        secretOf.set(secret, { name });
        return secret;
    };

    const ServerSecrets = define(class { constructor(key) { guard(key, "ServerSecrets"); } }, "ServerSecrets");
    getter(ServerSecrets, "names", function () {
        check("Native property getter", "ServerSecrets.names");
        return native.secretNames();
    });
    method(ServerSecrets, "get", function (name) {
        fn("ServerSecrets.get");
        if (typeof name !== "string")
            throw conversion();
        return native.hasSecret(name) ? makeSecret(name) : undefined;
    });

    const ServerVariables = define(class { constructor(key) { guard(key, "ServerVariables"); } }, "ServerVariables");
    getter(ServerVariables, "names", function () {
        check("Native property getter", "ServerVariables.names");
        return native.variableNames();
    });
    method(ServerVariables, "get", function (name) {
        fn("ServerVariables.get");
        if (typeof name !== "string")
            throw conversion();
        return native.variable(name);
    });

    const invalidArgument = (message, type, index) => {
        const error = new errors.classes.InvalidArgumentError(undefined, undefined, type, index);
        Object.defineProperty(error, "message", { value: message, writable: true, configurable: true });
        return error;
    };
    // AllowListEntryNameInfo or a Player: [name, xuid]; member names the call for an invalid player
    const details = (value, member) => {
        const state = native.playerState(value);
        if (state === 2)
            return native.playerInfo(value);
        if (state === 1)
            native.invalidPlayer(value, member);
        if (value === null || typeof value !== "object")
            throw conversion();
        const name = value.name === undefined ? "" : value.name;
        const xuid = value.xuid === undefined ? "" : value.xuid;
        if (typeof name !== "string" || typeof xuid !== "string")
            throw conversion();
        // sub_14E2E7450 (26.60): InvalidArgumentErrorType.Empty, argument 0
        if (name === "" && xuid === "")
            throw invalidArgument("Entry data must contain a name and/or a xuid", "Empty", 0);
        return [name, xuid];
    };
    const label = (info) => info[0] !== "" ? info[0] : info[1];

    const AllowList = define(class { constructor(key) { guard(key, "AllowList"); } }, "AllowList");
    getter(AllowList, "enabled", function () {
        check("Native property getter", "AllowList.enabled");
        return native.allowEnabled();
    }, function (value) {
        check("Native property setter", "AllowList.enabled");
        if (typeof value !== "boolean")
            throw conversion();
        native.setAllowEnabled(value);
    });
    getter(AllowList, "entries", function () {
        check("Native property getter", "AllowList.entries");
        return native.allowEntries().map(([name, xuid, ignoresPlayerLimit]) => {
            const entry = { ignoresPlayerLimit };
            if (name !== "")
                entry.name = name;
            if (xuid !== "")
                entry.xuid = xuid;
            return entry;
        });
    });
    method(AllowList, "add", function (playerDetails, ignorePlayerLimit = false) {
        fn("AllowList.add");
        const info = details(playerDetails, "add");
        if (typeof ignorePlayerLimit !== "boolean")
            throw conversion();
        if (!native.allowAdd(info[0], info[1], ignorePlayerLimit))
            throw errors.make("AllowListModificationError", "Unable to add player '" + label(info) + "' to allow list");
    });
    method(AllowList, "clear", function () {
        fn("AllowList.clear");
        native.allowClear();
    });
    method(AllowList, "contains", function (playerDetails) {
        fn("AllowList.contains");
        const info = details(playerDetails, "contains");
        return native.allowContains(info[0], info[1]);
    });
    method(AllowList, "reloadFile", function () {
        fn("AllowList.reloadFile");
        if (!native.allowReload())
            throw errors.make("AllowListFileReloadError", "Could not reload allow list file.");
    });
    method(AllowList, "remove", function (playerDetails) {
        fn("AllowList.remove");
        const info = details(playerDetails, "remove");
        if (!native.allowRemove(info[0], info[1]))
            throw errors.make("AllowListModificationError",
                "Unable to remove player '" + label(info) + "' from allow list");
    });

    const LevelStorageQuerySnapshotFile = define(class {
        constructor(key, fileName, fileSize) {
            guard(key, "LevelStorageQuerySnapshotFile");
            Object.defineProperty(this, "fileName", { value: fileName, enumerable: true });
            Object.defineProperty(this, "fileSize", { value: fileSize, enumerable: true });
        }
    }, "LevelStorageQuerySnapshotFile");
    const LevelStorage = define(class { constructor(key) { guard(key, "LevelStorage"); } }, "LevelStorage");
    const storageError = () => errors.make("LevelStorageSaveStateChangeError",
        "Level storage may be busy with previous commands.");
    method(LevelStorage, "saveHold", function () {
        fn("LevelStorage.saveHold");
        return native.saveHold() ? Promise.resolve() : Promise.reject(storageError());
    });
    method(LevelStorage, "saveQuery", function () {
        fn("LevelStorage.saveQuery");
        const files = native.saveQuery();
        if (files === undefined)
            return Promise.reject(storageError());
        return Promise.resolve(files.map(([name, size]) => new LevelStorageQuerySnapshotFile(token, name, size)));
    });
    method(LevelStorage, "saveResume", function () {
        fn("LevelStorage.saveResume");
        return native.saveResume() ? Promise.resolve() : Promise.reject(storageError());
    });

    const allowList = new AllowList(token);
    const levelStorage = new LevelStorage(token);
    const DedicatedServerUtils = define(class {
        constructor(key) { guard(key, "DedicatedServerUtils"); }
    }, "DedicatedServerUtils");
    getter(DedicatedServerUtils, "allowList", function () {
        check("Native property getter", "DedicatedServerUtils.allowList");
        return allowList;
    });
    getter(DedicatedServerUtils, "levelStorage", function () {
        check("Native property getter", "DedicatedServerUtils.levelStorage");
        return levelStorage;
    });
    for (const [name, call] of [["reloadCDNConfig", native.reloadCdn], ["reloadPermissions", native.reloadPermissions],
                                ["reloadScriptingConfig", native.reloadScripting], ["stopServer", native.stop]]) {
        method(DedicatedServerUtils, name, function () {
            fn("DedicatedServerUtils." + name);
            call();
        });
    }

    // the join event of one player; token names the join on the native side
    const joinOf = new WeakMap();
    const AsyncPlayerJoinBeforeEvent = define(class {
        constructor(key, join, name, persistentId) {
            guard(key, "AsyncPlayerJoinBeforeEvent");
            joinOf.set(this, join);
            Object.defineProperty(this, "name", { value: name, enumerable: true });
            Object.defineProperty(this, "persistentId", { value: persistentId, enumerable: true });
            // no split screen session: the game leaves it undefined (sub_14E2FAB20)
            Object.defineProperty(this, "splitScreenSlot", { value: undefined, enumerable: true });
        }
    }, "AsyncPlayerJoinBeforeEvent");
    const decide = (event, member, refuse, reason) => {
        fn("AsyncPlayerJoinBeforeEvent." + member);
        const join = joinOf.get(event);
        if (join === undefined)
            throw new ReferenceError("Native function [AsyncPlayerJoinBeforeEvent::" + member
                + "] object bound to prototype does not exist.");
        if (reason !== undefined && typeof reason !== "string")
            throw conversion();
        if (!native.joinDecide(join.token, refuse, reason === undefined ? "" : reason))
            throw errors.make("DisconnectedError", "Tried to disconnect client after client was already disconnected.",
                { id: join.persistentId });
    };
    method(AsyncPlayerJoinBeforeEvent, "allowJoin", function () {
        decide(this, "allowJoin", false);
    });
    method(AsyncPlayerJoinBeforeEvent, "disallowJoin", function (reason) {
        decide(this, "disallowJoin", true, reason);
    });
    method(AsyncPlayerJoinBeforeEvent, "disconnect", function (reason) {
        decide(this, "disconnect", true, reason);
    });
    method(AsyncPlayerJoinBeforeEvent, "isValid", function () {
        fn("AsyncPlayerJoinBeforeEvent.isValid");
        const join = joinOf.get(this);
        return join !== undefined && native.joinPending(join.token);
    });

    const joinCallbacks = [];
    const AsyncPlayerJoinBeforeEventSignal = define(class {
        constructor(key) { guard(key, "AsyncPlayerJoinBeforeEventSignal"); }
    }, "AsyncPlayerJoinBeforeEventSignal");
    method(AsyncPlayerJoinBeforeEventSignal, "subscribe", function (callback) {
        fn("AsyncPlayerJoinBeforeEventSignal.subscribe");
        if (typeof callback !== "function")
            throw conversion();
        joinCallbacks.push(callback);
        return callback;
    });
    method(AsyncPlayerJoinBeforeEventSignal, "unsubscribe", function (callback) {
        fn("AsyncPlayerJoinBeforeEventSignal.unsubscribe");
        const index = joinCallbacks.indexOf(callback);
        if (index < 0)
            return false;
        joinCallbacks.splice(index, 1);
        return true;
    });
    const asyncPlayerJoin = new AsyncPlayerJoinBeforeEventSignal(token);
    const AdminBeforeEvents = define(class {
        constructor(key) { guard(key, "AdminBeforeEvents"); }
    }, "AdminBeforeEvents");
    getter(AdminBeforeEvents, "asyncPlayerJoin", function () {
        check("Native property getter", "AdminBeforeEvents.asyncPlayerJoin");
        return asyncPlayerJoin;
    });

    // the player stays out until every callback's promise settled; one rejection turns them away
    const emitJoin = (joinToken, name, persistentId) => {
        if (joinCallbacks.length === 0)
            return false;
        const event = new AsyncPlayerJoinBeforeEvent(token, { token: joinToken, persistentId }, name, persistentId);
        const promises = joinCallbacks.slice().map((callback) => {
            try {
                return Promise.resolve(callback(event));
            } catch (error) {
                return Promise.reject(error);
            }
        });
        Promise.all(promises).then(() => native.joinSettle(joinToken, false), (error) => {
            if (error !== undefined)
                console.warn(String(error));
            native.joinSettle(joinToken, true);
        });
        return true;
    };

    const player = (value, member) => {
        if (native.playerState(value) === 0)
            throw conversion();
        if (native.playerState(value) === 1)
            throw errors.make("InvalidArgumentError", "Player is invalid");
        return value;
    };
    const kickPlayer = function (target, reason) {
        fn("kickPlayer");
        player(target);
        if (reason !== undefined && typeof reason !== "string")
            throw conversion();
        native.kick(target, reason === undefined ? "" : reason);
    };
    const opPlayer = function (target) {
        fn("opPlayer");
        player(target);
        if (!native.op(target))
            throw errors.make("PlayerAlreadyOpError", "Player already has op permissions");
    };
    const deopPlayer = function (target) {
        fn("deopPlayer");
        player(target);
        native.deop(target);
    };
    const transferPlayer = function (target, options) {
        fn("transferPlayer");
        if (native.playerState(target) === 0)
            throw conversion();
        if (options === null || typeof options !== "object")
            throw conversion();
        if (native.playerState(target) === 1)
            throw new Error("transferPlayer failed to get player.");
        if ("netherNetId" in options) {
            if (typeof options.netherNetId !== "string")
                throw conversion();
            // sub_14E310CC0 (26.60): InvalidArgumentErrorType.Unknown, argument 1
            if (!native.transfer(target, options.netherNetId, -1))
                throw invalidArgument("NetherNet Id provided '" + options.netherNetId
                    + "' does not match expected format.", "Unknown", 1);
            return;
        }
        if (typeof options.hostname !== "string" || typeof options.port !== "number" || !Number.isFinite(options.port))
            throw conversion();
        native.transfer(target, options.hostname, options.port);
    };

    return {
        AdminBeforeEvents, AllowList, AsyncPlayerJoinBeforeEvent, AsyncPlayerJoinBeforeEventSignal,
        DedicatedServerUtils, LevelStorage, LevelStorageQuerySnapshotFile, SecretString, ServerSecrets, ServerVariables,
        deopPlayer, kickPlayer, opPlayer, transferPlayer,
        beforeEvents: new AdminBeforeEvents(token),
        dedicatedServer: new DedicatedServerUtils(token),
        secrets: new ServerSecrets(token),
        variables: new ServerVariables(token),
        hooks: { emitJoin, secretOf, hasJoin: () => joinCallbacks.length !== 0 },
    };
})
)JS";

    // servers holding a save: hold, query and resume are one sequence per server
    std::unordered_set<const ServerNetworkHandler *> &heldSaves() {
        static std::unordered_set<const ServerNetworkHandler *> held;
        return held;
    }

    ScriptApi &apiOf(JSContext *ctx) {
        return *ScriptApi::fromContext(ctx);
    }

    std::string stringArg(JSContext *ctx, int argc, JSValueConst *argv, int index) {
        if (index >= argc)
            return std::string();
        const char *text = JS_ToCString(ctx, argv[index]);
        std::string value = text == nullptr ? std::string() : text;
        JS_FreeCString(ctx, text);
        return value;
    }

    const ScriptPackConfig &configOf(JSContext *ctx) {
        ScriptApi &api = apiOf(ctx);
        return api.host().getScriptEngine().packConfigs().get(api.scriptModuleUuid());
    }

    JSValue toJs(JSContext *ctx, const json::Value &value) {
        switch (value.mType) {
            case json::Value::Type::Boolean:
                return JS_NewBool(ctx, value.mBoolean);
            case json::Value::Type::Number:
                return JS_NewFloat64(ctx, value.mNumber);
            case json::Value::Type::String:
                return JS_NewStringLen(ctx, value.mString.data(), value.mString.size());
            case json::Value::Type::Array: {
                JSValue array = JS_NewArray(ctx);
                for (size_t index = 0; index < value.mArray.size(); ++index)
                    JS_SetPropertyUint32(ctx, array, (uint32_t) index, toJs(ctx, *value.mArray[index]));
                return array;
            }
            case json::Value::Type::Object: {
                JSValue object = JS_NewObject(ctx);
                for (const std::string &key: value.mKeys)
                    JS_SetPropertyStr(ctx, object, key.c_str(), toJs(ctx, *value.get(key)));
                return object;
            }
            default:
                return JS_NULL;
        }
    }

    JSValue stringList(JSContext *ctx, const std::vector<std::string> &names) {
        JSValue array = JS_NewArray(ctx);
        for (size_t index = 0; index < names.size(); ++index)
            JS_SetPropertyUint32(ctx, array, (uint32_t) index, JS_NewString(ctx, names[index].c_str()));
        return array;
    }

    JSValue privilege(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        switch (apiOf(ctx).privilege()) {
            case ScriptPrivilege::Early:
                return JS_NewInt32(ctx, 1);
            case ScriptPrivilege::Restricted:
                return JS_NewInt32(ctx, 2);
            default:
                return JS_NewInt32(ctx, 0);
        }
    }

    JSValue flags(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        const std::string member = stringArg(ctx, argc, argv, 0);
        const ScriptSurface &surface = apiOf(ctx).adminSurface();
        JSValue array = JS_NewArray(ctx);
        JS_SetPropertyUint32(ctx, array, 0, JS_NewBool(ctx, surface.isEarly(member)));
        JS_SetPropertyUint32(ctx, array, 1, JS_NewBool(ctx, surface.isBlocked(member)));
        return array;
    }

    JSValue variableNames(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        std::vector<std::string> names;
        for (const auto &entry: configOf(ctx).mVariables)
            names.push_back(entry.first);
        return stringList(ctx, names);
    }

    JSValue variable(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        const json::Value *value = configOf(ctx).findVariable(stringArg(ctx, argc, argv, 0));
        return value == nullptr ? JS_UNDEFINED : toJs(ctx, *value);
    }

    JSValue secretNames(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        std::vector<std::string> names;
        for (const auto &entry: configOf(ctx).mSecrets)
            names.push_back(entry.first);
        return stringList(ctx, names);
    }

    JSValue hasSecret(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        return JS_NewBool(ctx, configOf(ctx).findSecret(stringArg(ctx, argc, argv, 0)) != nullptr);
    }

    // 0: not a Player, 1: a Player that left, 2: an online player
    JSValue playerState(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi &api = apiOf(ctx);
        if (argc < 1 || !JS_IsObject(argv[0]))
            return JS_NewInt32(ctx, 0);
        const ScriptEntityRef *ref = api.native<ScriptEntityRef>(argv[0]);
        if (ref == nullptr || ref->mKind != ScriptEntityRef::Kind::Player)
            return JS_NewInt32(ctx, 0);
        return JS_NewInt32(ctx, api.resolvePlayer(argv[0]) == nullptr ? 1 : 2);
    }

    JSValue playerInfo(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ServerPlayer *player = argc < 1 ? nullptr : apiOf(ctx).resolvePlayer(argv[0]);
        if (player == nullptr)
            return JS_UNDEFINED;
        return stringList(ctx, {player->getName(), player->getXuid()});
    }

    JSValue allowEnabled(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return JS_NewBool(ctx, apiOf(ctx).host().getProperties().getAllowList());
    }

    JSValue setAllowEnabled(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        apiOf(ctx).host().setAllowListEnabled(argc >= 1 && JS_ToBool(ctx, argv[0]) == 1);
        return JS_UNDEFINED;
    }

    JSValue allowEntries(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const AllowListEntry &entry: apiOf(ctx).host().getAllowList().getEntries()) {
            JSValue row = stringList(ctx, {entry.mName, entry.mXuid});
            JS_SetPropertyUint32(ctx, row, 2, JS_NewBool(ctx, entry.mIgnoresPlayerLimit));
            JS_SetPropertyUint32(ctx, array, index++, row);
        }
        return array;
    }

    JSValue allowAdd(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        const bool ignores = argc >= 3 && JS_ToBool(ctx, argv[2]) == 1;
        return JS_NewBool(ctx, apiOf(ctx).host().getAllowList().addEntry(stringArg(ctx, argc, argv, 0),
                                                                         stringArg(ctx, argc, argv, 1), ignores));
    }

    JSValue allowRemove(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ServerNetworkHandler &host = apiOf(ctx).host();
        const bool removed = host.getAllowList().removeEntry(stringArg(ctx, argc, argv, 0),
                                                             stringArg(ctx, argc, argv, 1));
        if (removed)
            host.kickNotAllowListedPlayers();
        return JS_NewBool(ctx, removed);
    }

    JSValue allowContains(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        return JS_NewBool(ctx, apiOf(ctx).host().getAllowList().contains(stringArg(ctx, argc, argv, 0),
                                                                         stringArg(ctx, argc, argv, 1)));
    }

    JSValue allowClear(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        ServerNetworkHandler &host = apiOf(ctx).host();
        host.getAllowList().clear();
        host.kickNotAllowListedPlayers();
        return JS_UNDEFINED;
    }

    JSValue allowReload(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        ServerNetworkHandler &host = apiOf(ctx).host();
        const bool reloaded = host.getAllowList().reload();
        if (reloaded)
            host.kickNotAllowListedPlayers();
        return JS_NewBool(ctx, reloaded);
    }

    JSValue kick(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi &api = apiOf(ctx);
        ServerPlayer *player = argc < 1 ? nullptr : api.resolvePlayer(argv[0]);
        if (player == nullptr)
            return JS_UNDEFINED;
        const std::string reason = stringArg(ctx, argc, argv, 1);
        api.host()._disconnect(player->getNetworkIdentifier(), DisconnectReason::Kicked,
                               reason.empty() ? player->localize("falcon.disconnect.kicked") : reason);
        return JS_UNDEFINED;
    }

    // false: already an operator
    JSValue op(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi &api = apiOf(ctx);
        ServerPlayer *player = argc < 1 ? nullptr : api.resolvePlayer(argv[0]);
        if (player == nullptr)
            return JS_TRUE;
        if (player->isOp())
            return JS_FALSE;
        api.host().getOpList().addOp(player->getName(), player->getXuid());
        api.host().setPlayerOp(*player, true);
        return JS_TRUE;
    }

    JSValue deop(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi &api = apiOf(ctx);
        ServerPlayer *player = argc < 1 ? nullptr : api.resolvePlayer(argv[0]);
        if (player == nullptr)
            return JS_UNDEFINED;
        api.host().getOpList().removeOp(player->getName());
        api.host().setPlayerOp(*player, false);
        return JS_UNDEFINED;
    }

    // sub_14202EC10 (26.60): a decimal uint64 or a uuid, written back the way sub_14201AB90 prints it
    bool canonicalNetherNetId(const std::string &text, std::string &out) {
        if (text.empty())
            return false;
        if (text.find_first_not_of("0123456789") == std::string::npos) {
            uint64_t value = 0;
            for (char c: text) {
                const uint64_t digit = (uint64_t) (c - '0');
                if (value > (UINT64_MAX - digit) / 10)
                    return false;
                value = value * 10 + digit;
            }
            out = std::to_string(value);
            return true;
        }
        std::string hex;
        if (text.size() == 36) {
            for (size_t index = 0; index < text.size(); ++index) {
                const bool dash = index == 8 || index == 13 || index == 18 || index == 23;
                if (dash != (text[index] == '-'))
                    return false;
                if (!dash)
                    hex.push_back(text[index]);
            }
        } else if (text.size() == 32) {
            hex = text;
        } else {
            return false;
        }
        for (char &c: hex) {
            if (!std::isxdigit((unsigned char) c))
                return false;
            c = (char) std::tolower((unsigned char) c);
        }
        out = hex.substr(0, 8) + "-" + hex.substr(8, 4) + "-" + hex.substr(12, 4) + "-" + hex.substr(16, 4) + "-"
              + hex.substr(20);
        return true;
    }

    // port -1: a NetherNet id in place of the host; false for an id the game refuses
    JSValue transfer(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi &api = apiOf(ctx);
        ServerPlayer *player = argc < 1 ? nullptr : api.resolvePlayer(argv[0]);
        double port = 0.0;
        if (player == nullptr || argc < 3 || JS_ToFloat64(ctx, &port, argv[2]) != 0)
            return JS_TRUE;
        TransferPacket packet;
        packet.mAddress = stringArg(ctx, argc, argv, 1);
        if (port < 0.0) {
            // the NetherNet form leaves the port at 0 (sub_143D7C340)
            if (!canonicalNetherNetId(packet.mAddress, packet.mAddress))
                return JS_FALSE;
            packet.mPort = 0;
        } else {
            packet.mPort = (int32_t) (uint16_t) (int64_t) port;
        }
        api.host().sendPacketTo(player->getNetworkIdentifier(), packet);
        return JS_TRUE;
    }

    // throws the InvalidEntityError the game raises for a Player that left
    JSValue invalidPlayer(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi &api = apiOf(ctx);
        const ScriptEntityRef *ref = argc < 1 ? nullptr : api.native<ScriptEntityRef>(argv[0]);
        if (ref == nullptr)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        return api.throwInvalidEntity("call", stringArg(ctx, argc, argv, 1), *ref);
    }

    JSValue reloadCdn(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        apiOf(ctx).host().getResourcePacks().loadCdnConfig(ServerPaths::file("cdn_config.json"));
        return JS_UNDEFINED;
    }

    JSValue reloadPermissions(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        ServerNetworkHandler &host = apiOf(ctx).host();
        host.getOpList().reload();
        for (auto &entry: host.getPlayers()) {
            ServerPlayer &player = entry.second;
            if (!player.isSpawned())
                continue;
            const bool op = host.getOpList().isOp(player.getName(), player.getXuid());
            if (op != player.isOp())
                host.setPlayerOp(player, op);
        }
        return JS_UNDEFINED;
    }

    JSValue reloadScripting(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        apiOf(ctx).host().getScriptEngine().packConfigs().reload();
        return JS_UNDEFINED;
    }

    JSValue stop(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        apiOf(ctx).host().requestStop();
        return JS_UNDEFINED;
    }

    // save hold: everything flushed, then no more writes until save resume
    JSValue saveHold(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        ServerNetworkHandler &host = apiOf(ctx).host();
        if (!heldSaves().insert(&host).second)
            return JS_FALSE;
        host.autoSave(true);
        host.setAutoSaveEnabled(false);
        return JS_TRUE;
    }

    // every file of the loaded worlds, named from the worlds folder
    JSValue saveQuery(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        ServerNetworkHandler &host = apiOf(ctx).host();
        if (heldSaves().count(&host) == 0)
            return JS_UNDEFINED;
        const std::filesystem::path worlds(ServerPaths::file("worlds"));
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (World *world: host.getWorlds().getWorlds()) {
            const std::filesystem::path folder = worlds / world->getName();
            // the non-throwing increment: a file the chunk saver removes mid-walk must not unwind through QuickJS
            std::error_code walk;
            std::filesystem::recursive_directory_iterator it(folder, walk);
            for (const std::filesystem::recursive_directory_iterator end; !walk && it != end; it.increment(walk)) {
                std::error_code error;
                if (!it->is_regular_file(error))
                    continue;
                const std::string name = it->path().lexically_relative(worlds).generic_string();
                JSValue row = JS_NewArray(ctx);
                JS_SetPropertyUint32(ctx, row, 0, JS_NewString(ctx, name.c_str()));
                JS_SetPropertyUint32(ctx, row, 1, JS_NewFloat64(ctx, (double) it->file_size(error)));
                JS_SetPropertyUint32(ctx, array, index++, row);
            }
        }
        return array;
    }

    JSValue saveResume(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        ServerNetworkHandler &host = apiOf(ctx).host();
        if (heldSaves().erase(&host) == 0)
            return JS_FALSE;
        host.setAutoSaveEnabled(true);
        return JS_TRUE;
    }

    uint32_t tokenArg(JSContext *ctx, int argc, JSValueConst *argv) {
        uint32_t token = 0;
        if (argc >= 1)
            JS_ToUint32(ctx, &token, argv[0]);
        return token;
    }

    JSValue joinPending(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        return JS_NewBool(ctx, apiOf(ctx).host().getScriptEngine().isJoinPending(tokenArg(ctx, argc, argv)));
    }

    JSValue joinDecide(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        const bool refuse = argc >= 2 && JS_ToBool(ctx, argv[1]) == 1;
        return JS_NewBool(ctx, apiOf(ctx).host().getScriptEngine().decideJoin(tokenArg(ctx, argc, argv), refuse,
                                                                              stringArg(ctx, argc, argv, 2)));
    }

    JSValue joinSettle(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        apiOf(ctx).host().getScriptEngine().settleJoin(tokenArg(ctx, argc, argv),
                                                       argc >= 2 && JS_ToBool(ctx, argv[1]) == 1);
        return JS_UNDEFINED;
    }

    void add(JSContext *ctx, JSValue object, const char *name, JSCFunction *function, int length) {
        JS_SetPropertyStr(ctx, object, name, JS_NewCFunction(ctx, function, name, length));
    }
}

namespace ScriptAdminBinding {
    const std::vector<const char *> &exportNames() {
        static const std::vector<const char *> names = {"AdminBeforeEvents",
                                                        "AllowList",
                                                        "AsyncPlayerJoinBeforeEvent",
                                                        "AsyncPlayerJoinBeforeEventSignal",
                                                        "DedicatedServerUtils",
                                                        "LevelStorage",
                                                        "LevelStorageQuerySnapshotFile",
                                                        "SecretString",
                                                        "ServerSecrets",
                                                        "ServerVariables",
                                                        "deopPlayer",
                                                        "kickPlayer",
                                                        "opPlayer",
                                                        "transferPlayer",
                                                        "beforeEvents",
                                                        "dedicatedServer",
                                                        "secrets",
                                                        "variables"};
        return names;
    }

    JSValue install(ScriptApi &api) {
        JSContext *ctx = api.context();
        JSValue native = JS_NewObject(ctx);
        add(ctx, native, "privilege", privilege, 0);
        add(ctx, native, "flags", flags, 1);
        add(ctx, native, "variableNames", variableNames, 0);
        add(ctx, native, "variable", variable, 1);
        add(ctx, native, "secretNames", secretNames, 0);
        add(ctx, native, "hasSecret", hasSecret, 1);
        add(ctx, native, "playerState", playerState, 1);
        add(ctx, native, "playerInfo", playerInfo, 1);
        add(ctx, native, "allowEnabled", allowEnabled, 0);
        add(ctx, native, "setAllowEnabled", setAllowEnabled, 1);
        add(ctx, native, "allowEntries", allowEntries, 0);
        add(ctx, native, "allowAdd", allowAdd, 3);
        add(ctx, native, "allowRemove", allowRemove, 2);
        add(ctx, native, "allowContains", allowContains, 2);
        add(ctx, native, "allowClear", allowClear, 0);
        add(ctx, native, "allowReload", allowReload, 0);
        add(ctx, native, "kick", kick, 2);
        add(ctx, native, "op", op, 1);
        add(ctx, native, "deop", deop, 1);
        add(ctx, native, "transfer", transfer, 3);
        add(ctx, native, "invalidPlayer", invalidPlayer, 2);
        add(ctx, native, "reloadCdn", reloadCdn, 0);
        add(ctx, native, "reloadPermissions", reloadPermissions, 0);
        add(ctx, native, "reloadScripting", reloadScripting, 0);
        add(ctx, native, "stop", stop, 0);
        add(ctx, native, "saveHold", saveHold, 0);
        add(ctx, native, "saveQuery", saveQuery, 0);
        add(ctx, native, "saveResume", saveResume, 0);
        add(ctx, native, "joinPending", joinPending, 1);
        add(ctx, native, "joinDecide", joinDecide, 3);
        add(ctx, native, "joinSettle", joinSettle, 2);
        JS_SetPropertyStr(ctx, native, "errors", JS_DupValue(ctx, api.errors()));

        JSValue factory = JS_Eval(ctx, ADMIN, std::strlen(ADMIN), "<admin>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(factory)) {
            JS_FreeValue(ctx, native);
            api.logPendingException("<admin>");
            return JS_NewObject(ctx);
        }
        JSValue result = JS_Call(ctx, factory, JS_UNDEFINED, 1, &native);
        JS_FreeValue(ctx, factory);
        JS_FreeValue(ctx, native);
        if (JS_IsException(result)) {
            api.logPendingException("<admin>");
            return JS_NewObject(ctx);
        }
        return result;
    }
}
