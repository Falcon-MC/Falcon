#include "Scripting/Binding/ScriptNetBinding.h"

#include "Core/Archive/Gzip.h"
#include "Core/NBT/NbtIo.h"
#include "Core/Net/NetUri.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/Binding/ScriptApi.h"
#include "Scripting/Net/ScriptNetService.h"
#include "Scripting/ScriptEngine.h"
#include "Scripting/ScriptStructureRegistry.h"
#include "World/Level/Structure/McStructure.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <deque>
#include <initializer_list>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace {
    const char *const NET = R"JS(
(function (native, secretOf) {
    const token = Symbol();
    const errors = native.errors;
    const conversion = () => new TypeError("Native type conversion failed.");
    const guard = (key, name) => {
        if (key !== token)
            throw new ReferenceError("No constructor for native class '" + name + "'.");
    };
    // the privilege tags of the definitions: early-execution-allowed and no-restricted-execution
    const check = (label, member) => {
        // indexed, not destructured: a pack can replace the array iterator
        const tags = native.flags(member);
        const early = tags[0], blocked = tags[1];
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
    const enumOf = (name) => {
        const values = {};
        for (const [key, value] of native.enumValues(name))
            values[key] = value;
        return Object.freeze(values);
    };
    const fail = (descriptor) => errors.make(descriptor[0], descriptor[1], descriptor[2]);

    const HttpRequestMethod = enumOf("HttpRequestMethod");
    const HttpStatusCode = enumOf("HttpStatusCode");
    const PacketId = enumOf("PacketId");
    const WebSocketClientCloseReasons = enumOf("WebSocketClientCloseReasons");
    const methods = Object.values(HttpRequestMethod);
    const packetIds = new Set(Object.values(PacketId));
    const isSecret = (value) => secretOf !== undefined && secretOf.has(value);
    const isHeaderValue = (value) => typeof value === "string" || isSecret(value);

    const headerOf = new WeakMap();
    const HttpHeader = define(class {
        constructor(key, value) {
            check("Native constructor for", "HttpHeader.constructor");
            if (arguments.length < 2 || typeof key !== "string" || !isHeaderValue(value))
                throw conversion();
            headerOf.set(this, { key, value });
        }
    }, "HttpHeader");
    getter(HttpHeader, "key", function () {
        return headerOf.get(this).key;
    }, function (value) {
        if (typeof value !== "string")
            throw conversion();
        headerOf.get(this).key = value;
    });
    getter(HttpHeader, "value", function () {
        return headerOf.get(this).value;
    }, function (value) {
        if (!isHeaderValue(value))
            throw conversion();
        headerOf.get(this).value = value;
    });
    const makeHeader = (key, value) => {
        const header = Object.create(HttpHeader.prototype);
        headerOf.set(header, { key, value });
        return header;
    };
    const headerList = (headers) => {
        if (!Array.isArray(headers) || headers.some((header) => !headerOf.has(header)))
            throw conversion();
        return headers.slice();
    };

    const requestOf = new WeakMap();
    const isBody = (body) => typeof body === "string" || native.isSerializable(body);
    const HttpRequest = define(class {
        constructor(uri) {
            check("Native constructor for", "HttpRequest.constructor");
            if (arguments.length < 1 || typeof uri !== "string")
                throw conversion();
            requestOf.set(this, { uri, method: HttpRequestMethod.Get, body: "", headers: [], timeout: 0 });
        }
    }, "HttpRequest");
    const field = (name, valid, convert) => getter(HttpRequest, name, function () {
        const value = requestOf.get(this)[name];
        return Array.isArray(value) ? value.slice() : value;
    }, function (value) {
        if (!valid(value))
            throw conversion();
        requestOf.get(this)[name] = convert === undefined ? value : convert(value);
    });
    field("uri", (value) => typeof value === "string");
    field("method", (value) => methods.includes(value));
    field("body", isBody);
    field("headers", (value) => Array.isArray(value) && value.every((header) => headerOf.has(header)),
        (value) => value.slice());
    field("timeout", (value) => typeof value === "number");
    method(HttpRequest, "addHeader", function (key, value) {
        fn("HttpRequest.addHeader");
        if (typeof key !== "string" || !isHeaderValue(value))
            throw conversion();
        requestOf.get(this).headers.push(makeHeader(key, value));
        return this;
    });
    method(HttpRequest, "setBody", function (body) {
        fn("HttpRequest.setBody");
        if (!isBody(body))
            throw conversion();
        requestOf.get(this).body = body;
        return this;
    });
    method(HttpRequest, "setHeaders", function (headers) {
        fn("HttpRequest.setHeaders");
        requestOf.get(this).headers = headerList(headers);
        return this;
    });
    method(HttpRequest, "setMethod", function (value) {
        fn("HttpRequest.setMethod");
        if (!methods.includes(value))
            throw conversion();
        requestOf.get(this).method = value;
        return this;
    });
    method(HttpRequest, "setTimeout", function (timeout) {
        fn("HttpRequest.setTimeout");
        if (typeof timeout !== "number")
            throw conversion();
        requestOf.get(this).timeout = timeout;
        return this;
    });

    // a header value for the native side: secrets stay a name of secrets.json or the script's own value
    const wireHeaders = (headers) => headers.map((header) => {
        const { key, value } = headerOf.get(header);
        return [key, typeof value === "string" ? value : secretOf.get(value)];
    });

    const responseOf = new WeakMap();
    const HttpResponse = define(class { constructor(key) { guard(key, "HttpResponse"); } }, "HttpResponse");
    for (const name of ["body", "headers", "request", "status"]) {
        getter(HttpResponse, name, function () {
            const value = responseOf.get(this)[name];
            return Array.isArray(value) ? value.slice() : value;
        });
    }
    method(HttpResponse, "deserialize", function (identifier) {
        fn("HttpResponse.deserialize");
        if (typeof identifier !== "string")
            throw conversion();
        const result = native.deserialize(responseOf.get(this).holder, identifier);
        if (Array.isArray(result))
            throw fail(result);
        return result;
    });

    let nextId = 1;
    const pending = new Map();
    const HttpClient = define(class { constructor(key) { guard(key, "HttpClient"); } }, "HttpClient");
    const send = (request) => {
        const id = nextId++;
        const state = requestOf.get(request);
        return new Promise((resolve, reject) => {
            const refused = native.request(id, state.uri, state.method, wireHeaders(state.headers), state.body,
                state.timeout);
            if (refused !== undefined) {
                reject(fail(refused));
                return;
            }
            pending.set(id, { resolve, reject, request });
        });
    };
    method(HttpClient, "cancelAll", function (reason) {
        fn("HttpClient.cancelAll");
        if (typeof reason !== "string")
            throw conversion();
        // sub_14B88F7F0: every pending promise is rejected with a plain Error carrying the reason
        const cancelled = Array.from(pending.values());
        pending.clear();
        native.cancelAll();
        for (const entry of cancelled)
            entry.reject(new Error(reason));
    });
    method(HttpClient, "get", function (uri) {
        fn("HttpClient.get");
        if (typeof uri !== "string")
            throw conversion();
        const request = Object.create(HttpRequest.prototype);
        requestOf.set(request, { uri, method: HttpRequestMethod.Get, body: "", headers: [], timeout: 0 });
        return send(request);
    });
    method(HttpClient, "request", function (config) {
        fn("HttpClient.request");
        if (!requestOf.has(config))
            throw conversion();
        return send(config);
    });

    const httpDone = (id, status, headers, body, holder) => {
        const entry = pending.get(id);
        if (entry === undefined)
            return;
        pending.delete(id);
        const response = new HttpResponse(token);
        responseOf.set(response, {
            body, status, request: entry.request, holder,
            headers: headers.map(([key, value]) => makeHeader(key, value)),
        });
        entry.resolve(response);
    };
    const httpFail = (id, name, message, fields) => {
        const entry = pending.get(id);
        if (entry === undefined)
            return;
        pending.delete(id);
        entry.reject(errors.make(name, message, fields));
    };

    // one subscriber list per signal; callbacks get the event, nothing comes back
    const makeSignal = (name, owner) => {
        const signal = define(class { constructor(key) { guard(key, name); } }, name);
        method(signal, "subscribe", function (callback) {
            fn(name + ".subscribe");
            if (typeof callback !== "function")
                throw conversion();
            owner(this).push(callback);
            return callback;
        });
        method(signal, "unsubscribe", function (callback) {
            fn(name + ".unsubscribe");
            if (typeof callback !== "function")
                throw conversion();
            const list = owner(this);
            const index = list.indexOf(callback);
            if (index >= 0)
                list.splice(index, 1);
        });
        return signal;
    };
    const run = (callbacks, event) => {
        for (const callback of callbacks.slice()) {
            try {
                callback(event);
            } catch (error) {
                native.report(error);
            }
        }
    };

    const socketOf = new WeakMap();
    const signalLists = new WeakMap();
    const listOf = (signal) => signalLists.get(signal);
    const CloseAfterEventSignal = makeSignal("CloseAfterEventSignal", listOf);
    const MessageAfterEventSignal = makeSignal("MessageAfterEventSignal", listOf);
    const WebSocketClientAfterEvents = define(class {
        constructor(key) { guard(key, "WebSocketClientAfterEvents"); }
    }, "WebSocketClientAfterEvents");
    const eventsOf = new WeakMap();
    getter(WebSocketClientAfterEvents, "close", function () {
        return eventsOf.get(this).close;
    });
    getter(WebSocketClientAfterEvents, "message", function () {
        return eventsOf.get(this).message;
    });
    const WebSocketClientCloseAfterEvent = define(class {
        constructor(key, reason) {
            guard(key, "WebSocketClientCloseAfterEvent");
            Object.defineProperty(this, "reason", { value: reason, enumerable: true });
        }
    }, "WebSocketClientCloseAfterEvent");
    const WebSocketClientReceiveAfterEvent = define(class {
        constructor(key, message) {
            guard(key, "WebSocketClientReceiveAfterEvent");
            Object.defineProperty(this, "message", { value: message, enumerable: true });
        }
    }, "WebSocketClientReceiveAfterEvent");
    const WebSocketClient = define(class {
        constructor(key, id) {
            guard(key, "WebSocketClient");
            const close = new CloseAfterEventSignal(token);
            const message = new MessageAfterEventSignal(token);
            signalLists.set(close, []);
            signalLists.set(message, []);
            const afterEvents = new WebSocketClientAfterEvents(token);
            eventsOf.set(afterEvents, { close, message });
            socketOf.set(this, { id, open: true, afterEvents, close, message });
        }
    }, "WebSocketClient");
    getter(WebSocketClient, "afterEvents", function () {
        return socketOf.get(this).afterEvents;
    });
    getter(WebSocketClient, "isOpen", function () {
        return socketOf.get(this).open;
    });
    const notConnected = () => errors.make("WebSocketNotConnectedError", "This web socket is not connected.");
    method(WebSocketClient, "close", function () {
        fn("WebSocketClient.close");
        const state = socketOf.get(this);
        if (!state.open)
            throw notConnected();
        state.open = false;
        native.wsClose(state.id);
    });
    method(WebSocketClient, "send", function (payload) {
        fn("WebSocketClient.send");
        if (typeof payload !== "string")
            throw conversion();
        const state = socketOf.get(this);
        if (!state.open)
            throw notConnected();
        const refused = native.wsSend(state.id, payload);
        if (refused !== undefined)
            throw fail(refused);
    });

    const connecting = new Map();
    const clients = new Map();
    const WebSocket = define(class { constructor(key) { guard(key, "WebSocket"); } }, "WebSocket");
    method(WebSocket, "connect", function (uri, headers) {
        fn("WebSocket.connect");
        if (typeof uri !== "string")
            throw conversion();
        const list = headers === undefined ? [] : headerList(headers);
        const id = nextId++;
        return new Promise((resolve, reject) => {
            const refused = native.wsConnect(id, uri, wireHeaders(list));
            if (refused !== undefined) {
                reject(fail(refused));
                return;
            }
            connecting.set(id, { resolve, reject });
        });
    });
    const wsOpened = (id) => {
        const entry = connecting.get(id);
        if (entry === undefined)
            return;
        connecting.delete(id);
        const client = new WebSocketClient(token, id);
        clients.set(id, client);
        entry.resolve(client);
    };
    const wsFailed = (id, name, message, fields) => {
        const entry = connecting.get(id);
        if (entry === undefined)
            return;
        connecting.delete(id);
        entry.reject(errors.make(name, message, fields));
    };
    const wsMessage = (id, text) => {
        const client = clients.get(id);
        if (client !== undefined)
            run(listOf(socketOf.get(client).message), new WebSocketClientReceiveAfterEvent(token, text));
    };
    const wsClosed = (id, reason) => {
        const client = clients.get(id);
        if (client === undefined)
            return;
        clients.delete(id);
        const state = socketOf.get(client);
        state.open = false;
        run(listOf(state.close), new WebSocketClientCloseAfterEvent(token, reason));
    };

    // packet events: each subscription keeps its options; the native side only asks while one exists
    const packetLists = { receive: [], send: [] };
    const optionsOf = new WeakMap();
    const readOptions = (options) => {
        if (options === undefined)
            return undefined;
        if (options === null || typeof options !== "object")
            throw conversion();
        const list = (value) => {
            if (value === undefined)
                return undefined;
            if (!Array.isArray(value) || value.some((id) => !packetIds.has(id)))
                throw conversion();
            return new Set(value);
        };
        return { monitored: list(options.monitoredPacketIds), ignored: list(options.ignoredPacketIds) };
    };
    const accepts = (options, packetId) => options === undefined
        || ((options.monitored === undefined || options.monitored.has(packetId))
            && (options.ignored === undefined || !options.ignored.has(packetId)));
    const packetSignal = (name, kind) => {
        const signal = define(class { constructor(key) { guard(key, name); } }, name);
        method(signal, "subscribe", function (callback, options) {
            fn(name + ".subscribe");
            if (typeof callback !== "function")
                throw conversion();
            const read = readOptions(options);
            packetLists[kind].push({ callback, options: read });
            native.packetInterest(kind === "receive", true);
            return callback;
        });
        method(signal, "unsubscribe", function (callback) {
            fn(name + ".unsubscribe");
            if (typeof callback !== "function")
                throw conversion();
            const list = packetLists[kind];
            const index = list.findIndex((entry) => entry.callback === callback);
            if (index >= 0)
                list.splice(index, 1);
            native.packetInterest(kind === "receive", list.length !== 0);
        });
        return signal;
    };
    const PacketReceiveBeforeEventSignal = packetSignal("PacketReceiveBeforeEventSignal", "receive");
    const PacketSendBeforeEventSignal = packetSignal("PacketSendBeforeEventSignal", "send");
    const PacketReceivedBeforeEvent = define(class {
        constructor(key, packetId, packetSize, sender) {
            guard(key, "PacketReceivedBeforeEvent");
            this.cancel = false;
            Object.defineProperty(this, "packetId", { value: packetId, enumerable: true });
            Object.defineProperty(this, "packetSize", { value: packetSize, enumerable: true });
            Object.defineProperty(this, "sender", { value: sender, enumerable: true });
        }
    }, "PacketReceivedBeforeEvent");
    const PacketSendBeforeEvent = define(class {
        constructor(key, packetId, recipients) {
            guard(key, "PacketSendBeforeEvent");
            this.cancel = false;
            Object.defineProperty(this, "packetId", { value: packetId, enumerable: true });
            Object.defineProperty(this, "recipients", { value: recipients, enumerable: true });
        }
    }, "PacketSendBeforeEvent");
    const dispatch = (kind, packetId, event) => {
        let any = false;
        for (const entry of packetLists[kind].slice()) {
            if (!accepts(entry.options, packetId))
                continue;
            any = true;
            try {
                entry.callback(event);
            } catch (error) {
                native.report(error);
            }
        }
        return any && event.cancel === true;
    };
    const emitReceive = (packetId, size, sender) =>
        dispatch("receive", packetId, new PacketReceivedBeforeEvent(token, packetId, size, sender));
    const emitSend = (packetId, recipients) =>
        dispatch("send", packetId, new PacketSendBeforeEvent(token, packetId, recipients));

    const packetReceive = new PacketReceiveBeforeEventSignal(token);
    const packetSend = new PacketSendBeforeEventSignal(token);
    const NetworkBeforeEvents = define(class {
        constructor(key) { guard(key, "NetworkBeforeEvents"); }
    }, "NetworkBeforeEvents");
    getter(NetworkBeforeEvents, "packetReceive", function () {
        check("Native property getter", "NetworkBeforeEvents.packetReceive");
        return packetReceive;
    });
    getter(NetworkBeforeEvents, "packetSend", function () {
        check("Native property getter", "NetworkBeforeEvents.packetSend");
        return packetSend;
    });

    return {
        CloseAfterEventSignal, HttpClient, HttpHeader, HttpRequest, HttpRequestMethod, HttpResponse, HttpStatusCode,
        MessageAfterEventSignal, NetworkBeforeEvents, PacketId, PacketReceiveBeforeEventSignal,
        PacketReceivedBeforeEvent, PacketSendBeforeEvent, PacketSendBeforeEventSignal, WebSocket, WebSocketClient,
        WebSocketClientAfterEvents, WebSocketClientCloseAfterEvent, WebSocketClientCloseReasons,
        WebSocketClientReceiveAfterEvent,
        beforeEvents: new NetworkBeforeEvents(token),
        http: new HttpClient(token),
        websocket: new WebSocket(token),
        hooks: { httpDone, httpFail, wsOpened, wsFailed, wsMessage, wsClosed, emitReceive, emitSend },
    };
})
)JS";

    // the content type of the structure serializer, sub_14DE87990 (26.60)
    const char *const STRUCTURE_CONTENT_TYPE = "application/x-minecraft-nbt; type=structure";
    const char *const BODY_TOO_LARGE = "HTTP request or Web Socket message body exceeds allowed size.";

    struct PacketName {
        MinecraftPacketIds mId;
        const char *mName;
    };

    // the PacketId enum of the definitions: every member is the protocol name plus "Packet"
    const PacketName PACKET_NAMES[] = {
            {MinecraftPacketIds::ActorEvent, "ActorEventPacket"},
            {MinecraftPacketIds::ActorPickRequest, "ActorPickRequestPacket"},
            {MinecraftPacketIds::AddActor, "AddActorPacket"},
            {MinecraftPacketIds::AddBehaviorTree, "AddBehaviorTreePacket"},
            {MinecraftPacketIds::AddItemActor, "AddItemActorPacket"},
            {MinecraftPacketIds::AddPainting, "AddPaintingPacket"},
            {MinecraftPacketIds::AddPlayer, "AddPlayerPacket"},
            {MinecraftPacketIds::AddVolumeEntity, "AddVolumeEntityPacket"},
            {MinecraftPacketIds::AgentActionEvent, "AgentActionEventPacket"},
            {MinecraftPacketIds::AgentAnimation, "AgentAnimationPacket"},
            {MinecraftPacketIds::AnimateEntity, "AnimateEntityPacket"},
            {MinecraftPacketIds::Animate, "AnimatePacket"},
            {MinecraftPacketIds::AnvilDamage, "AnvilDamagePacket"},
            {MinecraftPacketIds::AutomationClientConnect, "AutomationClientConnectPacket"},
            {MinecraftPacketIds::AvailableActorIdentifiers, "AvailableActorIdentifiersPacket"},
            {MinecraftPacketIds::AvailableCommands, "AvailableCommandsPacket"},
            {MinecraftPacketIds::AwardAchievement, "AwardAchievementPacket"},
            {MinecraftPacketIds::BiomeDefinitionList, "BiomeDefinitionListPacket"},
            {MinecraftPacketIds::BlockActorData, "BlockActorDataPacket"},
            {MinecraftPacketIds::BlockEvent, "BlockEventPacket"},
            {MinecraftPacketIds::BlockPickRequest, "BlockPickRequestPacket"},
            {MinecraftPacketIds::BookEdit, "BookEditPacket"},
            {MinecraftPacketIds::BossEvent, "BossEventPacket"},
            {MinecraftPacketIds::CameraAimAssistActorPriority, "CameraAimAssistActorPriorityPacket"},
            {MinecraftPacketIds::CameraAimAssist, "CameraAimAssistPacket"},
            {MinecraftPacketIds::CameraAimAssistPresets, "CameraAimAssistPresetsPacket"},
            {MinecraftPacketIds::CameraInstruction, "CameraInstructionPacket"},
            {MinecraftPacketIds::Camera, "CameraPacket"},
            {MinecraftPacketIds::CameraPresets, "CameraPresetsPacket"},
            {MinecraftPacketIds::CameraShake, "CameraShakePacket"},
            {MinecraftPacketIds::CameraSpline, "CameraSplinePacket"},
            {MinecraftPacketIds::ChangeDimension, "ChangeDimensionPacket"},
            {MinecraftPacketIds::ChangeMobProperty, "ChangeMobPropertyPacket"},
            {MinecraftPacketIds::ChunkRadiusUpdated, "ChunkRadiusUpdatedPacket"},
            {MinecraftPacketIds::ClientboundAttributeLayerSync, "ClientboundAttributeLayerSyncPacket"},
            {MinecraftPacketIds::ClientboundCloseForm, "ClientboundCloseFormPacket"},
            {MinecraftPacketIds::ClientboundControlSchemeSet, "ClientboundControlSchemeSetPacket"},
            {MinecraftPacketIds::ClientboundDataDrivenUICloseScreen, "ClientboundDataDrivenUICloseScreenPacket"},
            {MinecraftPacketIds::ClientboundDataDrivenUIReload, "ClientboundDataDrivenUIReloadPacket"},
            {MinecraftPacketIds::ClientboundDataDrivenUIShowScreen, "ClientboundDataDrivenUIShowScreenPacket"},
            {MinecraftPacketIds::ClientboundDataStore, "ClientboundDataStorePacket"},
            {MinecraftPacketIds::ClientboundDebugRenderer, "ClientboundDebugRendererPacket"},
            {MinecraftPacketIds::ClientboundMapItemData, "ClientboundMapItemDataPacket"},
            {MinecraftPacketIds::ClientboundMatchmakingState, "ClientboundMatchmakingStatePacket"},
            {MinecraftPacketIds::ClientboundPlayAudioContent, "ClientboundPlayAudioContentPacket"},
            {MinecraftPacketIds::ClientboundStonecutterSetRecipe, "ClientboundStonecutterSetRecipePacket"},
            {MinecraftPacketIds::ClientboundTextureShift, "ClientboundTextureShiftPacket"},
            {MinecraftPacketIds::ClientboundUpdateSoundData, "ClientboundUpdateSoundDataPacket"},
            {MinecraftPacketIds::ClientCacheBlobStatus, "ClientCacheBlobStatusPacket"},
            {MinecraftPacketIds::ClientCacheMissResponse, "ClientCacheMissResponsePacket"},
            {MinecraftPacketIds::ClientCacheStatus, "ClientCacheStatusPacket"},
            {MinecraftPacketIds::ClientCameraAimAssist, "ClientCameraAimAssistPacket"},
            {MinecraftPacketIds::ClientMovementPredictionSync, "ClientMovementPredictionSyncPacket"},
            {MinecraftPacketIds::ClientToServerHandshake, "ClientToServerHandshakePacket"},
            {MinecraftPacketIds::CodeBuilder, "CodeBuilderPacket"},
            {MinecraftPacketIds::CodeBuilderSource, "CodeBuilderSourcePacket"},
            {MinecraftPacketIds::CommandBlockUpdate, "CommandBlockUpdatePacket"},
            {MinecraftPacketIds::CommandOutput, "CommandOutputPacket"},
            {MinecraftPacketIds::CommandRequest, "CommandRequestPacket"},
            {MinecraftPacketIds::CompletedUsingItem, "CompletedUsingItemPacket"},
            {MinecraftPacketIds::ContainerClose, "ContainerClosePacket"},
            {MinecraftPacketIds::ContainerOpen, "ContainerOpenPacket"},
            {MinecraftPacketIds::ContainerRegistryCleanup, "ContainerRegistryCleanupPacket"},
            {MinecraftPacketIds::ContainerSetData, "ContainerSetDataPacket"},
            {MinecraftPacketIds::CorrectPlayerMovePrediction, "CorrectPlayerMovePredictionPacket"},
            {MinecraftPacketIds::CraftingData, "CraftingDataPacket"},
            {MinecraftPacketIds::CreatePhoto, "CreatePhotoPacket"},
            {MinecraftPacketIds::CreativeContent, "CreativeContentPacket"},
            {MinecraftPacketIds::CurrentStructureFeature, "CurrentStructureFeaturePacket"},
            {MinecraftPacketIds::DeathInfo, "DeathInfoPacket"},
            {MinecraftPacketIds::DebugInfo, "DebugInfoPacket"},
            {MinecraftPacketIds::DimensionData, "DimensionDataPacket"},
            {MinecraftPacketIds::Disconnect, "DisconnectPacket"},
            {MinecraftPacketIds::EditorNetwork, "EditorNetworkPacket"},
            {MinecraftPacketIds::EducationSettings, "EducationSettingsPacket"},
            {MinecraftPacketIds::EduUriResource, "EduUriResourcePacket"},
            {MinecraftPacketIds::EmoteList, "EmoteListPacket"},
            {MinecraftPacketIds::Emote, "EmotePacket"},
            {MinecraftPacketIds::FeatureRegistry, "FeatureRegistryPacket"},
            {MinecraftPacketIds::GameRulesChanged, "GameRulesChangedPacket"},
            {MinecraftPacketIds::GameTestRequest, "GameTestRequestPacket"},
            {MinecraftPacketIds::GameTestResults, "GameTestResultsPacket"},
            {MinecraftPacketIds::GraphicsOverrideParameter, "GraphicsOverrideParameterPacket"},
            {MinecraftPacketIds::GuiDataPickItem, "GuiDataPickItemPacket"},
            {MinecraftPacketIds::HurtArmor, "HurtArmorPacket"},
            {MinecraftPacketIds::Interact, "InteractPacket"},
            {MinecraftPacketIds::InventoryContent, "InventoryContentPacket"},
            {MinecraftPacketIds::InventorySlot, "InventorySlotPacket"},
            {MinecraftPacketIds::InventoryTransaction, "InventoryTransactionPacket"},
            {MinecraftPacketIds::ItemRegistry, "ItemRegistryPacket"},
            {MinecraftPacketIds::ItemStackRequest, "ItemStackRequestPacket"},
            {MinecraftPacketIds::ItemStackResponse, "ItemStackResponsePacket"},
            {MinecraftPacketIds::JigsawStructureData, "JigsawStructureDataPacket"},
            {MinecraftPacketIds::LabTable, "LabTablePacket"},
            {MinecraftPacketIds::LecternUpdate, "LecternUpdatePacket"},
            {MinecraftPacketIds::LegacyTelemetryEvent, "LegacyTelemetryEventPacket"},
            {MinecraftPacketIds::LessonProgress, "LessonProgressPacket"},
            {MinecraftPacketIds::LevelChunk, "LevelChunkPacket"},
            {MinecraftPacketIds::LevelEventGeneric, "LevelEventGenericPacket"},
            {MinecraftPacketIds::LevelEvent, "LevelEventPacket"},
            {MinecraftPacketIds::LevelSoundEvent, "LevelSoundEventPacket"},
            {MinecraftPacketIds::LocatorBar, "LocatorBarPacket"},
            {MinecraftPacketIds::Login, "LoginPacket"},
            {MinecraftPacketIds::MapCreateLockedCopy, "MapCreateLockedCopyPacket"},
            {MinecraftPacketIds::MapInfoRequest, "MapInfoRequestPacket"},
            {MinecraftPacketIds::MobArmorEquipment, "MobArmorEquipmentPacket"},
            {MinecraftPacketIds::MobEffect, "MobEffectPacket"},
            {MinecraftPacketIds::MobEquipment, "MobEquipmentPacket"},
            {MinecraftPacketIds::ModalFormRequest, "ModalFormRequestPacket"},
            {MinecraftPacketIds::ModalFormResponse, "ModalFormResponsePacket"},
            {MinecraftPacketIds::MotionPredictionHints, "MotionPredictionHintsPacket"},
            {MinecraftPacketIds::MoveActorAbsolute, "MoveActorAbsolutePacket"},
            {MinecraftPacketIds::MoveActorDelta, "MoveActorDeltaPacket"},
            {MinecraftPacketIds::MovementEffect, "MovementEffectPacket"},
            {MinecraftPacketIds::MovePlayer, "MovePlayerPacket"},
            {MinecraftPacketIds::MultiplayerSettings, "MultiplayerSettingsPacket"},
            {MinecraftPacketIds::NetworkChunkPublisherUpdate, "NetworkChunkPublisherUpdatePacket"},
            {MinecraftPacketIds::NetworkSettings, "NetworkSettingsPacket"},
            {MinecraftPacketIds::NetworkStackLatency, "NetworkStackLatencyPacket"},
            {MinecraftPacketIds::NpcDialogue, "NpcDialoguePacket"},
            {MinecraftPacketIds::NpcRequest, "NpcRequestPacket"},
            {MinecraftPacketIds::OnScreenTextureAnimation, "OnScreenTextureAnimationPacket"},
            {MinecraftPacketIds::OpenSign, "OpenSignPacket"},
            {MinecraftPacketIds::PacketViolationWarning, "PacketViolationWarningPacket"},
            {MinecraftPacketIds::PartyChanged, "PartyChangedPacket"},
            {MinecraftPacketIds::PartyDestinationCookieResponse, "PartyDestinationCookieResponsePacket"},
            {MinecraftPacketIds::PhotoTransfer, "PhotoTransferPacket"},
            {MinecraftPacketIds::PlayerAction, "PlayerActionPacket"},
            {MinecraftPacketIds::PlayerArmorDamage, "PlayerArmorDamagePacket"},
            {MinecraftPacketIds::PlayerAuthInput, "PlayerAuthInputPacket"},
            {MinecraftPacketIds::PlayerEnchantOptions, "PlayerEnchantOptionsPacket"},
            {MinecraftPacketIds::PlayerFog, "PlayerFogPacket"},
            {MinecraftPacketIds::PlayerHotbar, "PlayerHotbarPacket"},
            {MinecraftPacketIds::PlayerList, "PlayerListPacket"},
            {MinecraftPacketIds::PlayerLocation, "PlayerLocationPacket"},
            {MinecraftPacketIds::PlayerSkin, "PlayerSkinPacket"},
            {MinecraftPacketIds::PlayerStartItemCooldown, "PlayerStartItemCooldownPacket"},
            {MinecraftPacketIds::PlayerToggleCrafterSlotRequest, "PlayerToggleCrafterSlotRequestPacket"},
            {MinecraftPacketIds::PlayerUpdateEntityOverrides, "PlayerUpdateEntityOverridesPacket"},
            {MinecraftPacketIds::PlaySound, "PlaySoundPacket"},
            {MinecraftPacketIds::PlayStatus, "PlayStatusPacket"},
            {MinecraftPacketIds::PositionTrackingDBClientRequest, "PositionTrackingDBClientRequestPacket"},
            {MinecraftPacketIds::PositionTrackingDBServerBroadcast, "PositionTrackingDBServerBroadcastPacket"},
            {MinecraftPacketIds::PrimitiveShapes, "PrimitiveShapesPacket"},
            {MinecraftPacketIds::PurchaseReceipt, "PurchaseReceiptPacket"},
            {MinecraftPacketIds::RecordStarted, "RecordStartedPacket"},
            {MinecraftPacketIds::RefreshEntitlements, "RefreshEntitlementsPacket"},
            {MinecraftPacketIds::RemoveActor, "RemoveActorPacket"},
            {MinecraftPacketIds::RemoveObjective, "RemoveObjectivePacket"},
            {MinecraftPacketIds::RemoveVolumeEntity, "RemoveVolumeEntityPacket"},
            {MinecraftPacketIds::RequestAbility, "RequestAbilityPacket"},
            {MinecraftPacketIds::RequestChunkRadius, "RequestChunkRadiusPacket"},
            {MinecraftPacketIds::RequestNetworkSettings, "RequestNetworkSettingsPacket"},
            {MinecraftPacketIds::RequestPermissions, "RequestPermissionsPacket"},
            {MinecraftPacketIds::ResourcePackChunkData, "ResourcePackChunkDataPacket"},
            {MinecraftPacketIds::ResourcePackChunkRequest, "ResourcePackChunkRequestPacket"},
            {MinecraftPacketIds::ResourcePackClientResponse, "ResourcePackClientResponsePacket"},
            {MinecraftPacketIds::ResourcePackDataInfo, "ResourcePackDataInfoPacket"},
            {MinecraftPacketIds::ResourcePacksInfo, "ResourcePacksInfoPacket"},
            {MinecraftPacketIds::ResourcePacksReadyForValidation, "ResourcePacksReadyForValidationPacket"},
            {MinecraftPacketIds::ResourcePackStack, "ResourcePackStackPacket"},
            {MinecraftPacketIds::Respawn, "RespawnPacket"},
            {MinecraftPacketIds::ScriptMessage, "ScriptMessagePacket"},
            {MinecraftPacketIds::SendPartyDestinationCookie, "SendPartyDestinationCookiePacket"},
            {MinecraftPacketIds::ServerboundCursorItemDrag, "ServerboundCursorItemDragPacket"},
            {MinecraftPacketIds::ServerboundDataDrivenScreenClosed, "ServerboundDataDrivenScreenClosedPacket"},
            {MinecraftPacketIds::ServerboundDataStore, "ServerboundDataStorePacket"},
            {MinecraftPacketIds::ServerboundDiagnostics, "ServerboundDiagnosticsPacket"},
            {MinecraftPacketIds::ServerboundLoadingScreen, "ServerboundLoadingScreenPacket"},
            {MinecraftPacketIds::ServerboundMatchmakingCancel, "ServerboundMatchmakingCancelPacket"},
            {MinecraftPacketIds::ServerboundPackSettingChange, "ServerboundPackSettingChangePacket"},
            {MinecraftPacketIds::ServerboundRegisterAudioContent, "ServerboundRegisterAudioContentPacket"},
            {MinecraftPacketIds::ServerboundStonecutterSetRecipe, "ServerboundStonecutterSetRecipePacket"},
            {MinecraftPacketIds::ServerPlayerPostMovePosition, "ServerPlayerPostMovePositionPacket"},
            {MinecraftPacketIds::ServerPresenceInfo, "ServerPresenceInfoPacket"},
            {MinecraftPacketIds::ServerSettingsRequest, "ServerSettingsRequestPacket"},
            {MinecraftPacketIds::ServerSettingsResponse, "ServerSettingsResponsePacket"},
            {MinecraftPacketIds::ServerStats, "ServerStatsPacket"},
            {MinecraftPacketIds::ServerStoreInfo, "ServerStoreInfoPacket"},
            {MinecraftPacketIds::ServerToClientHandshake, "ServerToClientHandshakePacket"},
            {MinecraftPacketIds::SetActorData, "SetActorDataPacket"},
            {MinecraftPacketIds::SetActorLink, "SetActorLinkPacket"},
            {MinecraftPacketIds::SetActorMotion, "SetActorMotionPacket"},
            {MinecraftPacketIds::SetCommandsEnabled, "SetCommandsEnabledPacket"},
            {MinecraftPacketIds::SetDefaultGameType, "SetDefaultGameTypePacket"},
            {MinecraftPacketIds::SetDifficulty, "SetDifficultyPacket"},
            {MinecraftPacketIds::SetDisplayObjective, "SetDisplayObjectivePacket"},
            {MinecraftPacketIds::SetHealth, "SetHealthPacket"},
            {MinecraftPacketIds::SetHud, "SetHudPacket"},
            {MinecraftPacketIds::SetLastHurtBy, "SetLastHurtByPacket"},
            {MinecraftPacketIds::SetLocalPlayerAsInitialized, "SetLocalPlayerAsInitializedPacket"},
            {MinecraftPacketIds::SetPassengerOfBlock, "SetPassengerOfBlockPacket"},
            {MinecraftPacketIds::SetPlayerFurnaceOptions, "SetPlayerFurnaceOptionsPacket"},
            {MinecraftPacketIds::SetPlayerGameType, "SetPlayerGameTypePacket"},
            {MinecraftPacketIds::SetPlayerInventoryOptions, "SetPlayerInventoryOptionsPacket"},
            {MinecraftPacketIds::SetScoreboardIdentity, "SetScoreboardIdentityPacket"},
            {MinecraftPacketIds::SetScore, "SetScorePacket"},
            {MinecraftPacketIds::SetSpawnPosition, "SetSpawnPositionPacket"},
            {MinecraftPacketIds::SetTime, "SetTimePacket"},
            {MinecraftPacketIds::SettingsCommand, "SettingsCommandPacket"},
            {MinecraftPacketIds::SetTitle, "SetTitlePacket"},
            {MinecraftPacketIds::ShowCredits, "ShowCreditsPacket"},
            {MinecraftPacketIds::ShowProfile, "ShowProfilePacket"},
            {MinecraftPacketIds::ShowStoreOffer, "ShowStoreOfferPacket"},
            {MinecraftPacketIds::SimpleEvent, "SimpleEventPacket"},
            {MinecraftPacketIds::SimulationType, "SimulationTypePacket"},
            {MinecraftPacketIds::SpawnExperienceOrb, "SpawnExperienceOrbPacket"},
            {MinecraftPacketIds::SpawnParticleEffect, "SpawnParticleEffectPacket"},
            {MinecraftPacketIds::StartGame, "StartGamePacket"},
            {MinecraftPacketIds::StopSound, "StopSoundPacket"},
            {MinecraftPacketIds::StructureBlockUpdate, "StructureBlockUpdatePacket"},
            {MinecraftPacketIds::StructureTemplateDataRequest, "StructureTemplateDataRequestPacket"},
            {MinecraftPacketIds::StructureTemplateDataResponse, "StructureTemplateDataResponsePacket"},
            {MinecraftPacketIds::SubChunk, "SubChunkPacket"},
            {MinecraftPacketIds::SubChunkRequest, "SubChunkRequestPacket"},
            {MinecraftPacketIds::SubClientLogin, "SubClientLoginPacket"},
            {MinecraftPacketIds::SyncActorProperty, "SyncActorPropertyPacket"},
            {MinecraftPacketIds::SyncWorldClocks, "SyncWorldClocksPacket"},
            {MinecraftPacketIds::TakeItemActor, "TakeItemActorPacket"},
            {MinecraftPacketIds::Text, "TextPacket"},
            {MinecraftPacketIds::TickingAreasLoadStatus, "TickingAreasLoadStatusPacket"},
            {MinecraftPacketIds::ToastRequest, "ToastRequestPacket"},
            {MinecraftPacketIds::Transfer, "TransferPacket"},
            {MinecraftPacketIds::TrimData, "TrimDataPacket"},
            {MinecraftPacketIds::UnlockedRecipes, "UnlockedRecipesPacket"},
            {MinecraftPacketIds::UpdateAbilities, "UpdateAbilitiesPacket"},
            {MinecraftPacketIds::UpdateAdventureSettings, "UpdateAdventureSettingsPacket"},
            {MinecraftPacketIds::UpdateAttributes, "UpdateAttributesPacket"},
            {MinecraftPacketIds::UpdateBlock, "UpdateBlockPacket"},
            {MinecraftPacketIds::UpdateBlockSynced, "UpdateBlockSyncedPacket"},
            {MinecraftPacketIds::UpdateClientInputLocks, "UpdateClientInputLocksPacket"},
            {MinecraftPacketIds::UpdateClientOptions, "UpdateClientOptionsPacket"},
            {MinecraftPacketIds::UpdateEquip, "UpdateEquipPacket"},
            {MinecraftPacketIds::UpdatePlayerGameType, "UpdatePlayerGameTypePacket"},
            {MinecraftPacketIds::UpdateSoftEnum, "UpdateSoftEnumPacket"},
            {MinecraftPacketIds::UpdateSubChunkBlocks, "UpdateSubChunkBlocksPacket"},
            {MinecraftPacketIds::UpdateTrade, "UpdateTradePacket"},
            {MinecraftPacketIds::VoxelShapes, "VoxelShapesPacket"},
    };

    // the response a deserialize call reads: the bytes as received, not the string the script sees
    struct ScriptNetBody : ScriptNative {
        std::string mBytes;
        std::string mContentType;
    };

    ScriptApi &apiOf(JSContext *ctx) {
        return *ScriptApi::fromContext(ctx);
    }

    std::string stringOf(JSContext *ctx, JSValueConst value) {
        size_t length = 0;
        const char *text = JS_ToCStringLen(ctx, &length, value);
        std::string out = text == nullptr ? std::string() : std::string(text, length);
        JS_FreeCString(ctx, text);
        return out;
    }

    std::string lower(std::string text) {
        for (char &c: text)
            c = (char) std::tolower((unsigned char) c);
        return text;
    }

    bool iequals(const std::string &a, const std::string &b) {
        return lower(a) == lower(b);
    }

    // sub_14B8291D0 (26.60): lowercase, spaces dropped except one between two words, none next to ';'
    std::string normalizeContentType(const std::string &value) {
        std::string out;
        bool afterSpace = false;
        for (char c: value) {
            if (c != ' ') {
                if (afterSpace && !out.empty() && c != ';' && out.back() != ';')
                    out.push_back(' ');
                out.push_back((char) std::tolower((unsigned char) c));
            }
            afterSpace = c == ' ';
        }
        return out;
    }

    // [class, message, fields]: the script side turns it into the error
    JSValue refusal(JSContext *ctx, const char *name, const std::string &message, JSValue fields = JS_UNDEFINED) {
        JSValue array = JS_NewArray(ctx);
        JS_SetPropertyUint32(ctx, array, 0, JS_NewString(ctx, name));
        JS_SetPropertyUint32(ctx, array, 1, JS_NewStringLen(ctx, message.data(), message.size()));
        JS_SetPropertyUint32(ctx, array, 2, JS_IsUndefined(fields) ? JS_NewObject(ctx) : fields);
        return array;
    }

    JSValue fieldsOf(JSContext *ctx, std::initializer_list<std::pair<const char *, JSValue>> values) {
        JSValue object = JS_NewObject(ctx);
        for (const auto &value: values)
            JS_SetPropertyStr(ctx, object, value.first, value.second);
        return object;
    }

    JSValue uriFields(JSContext *ctx, const std::string &uri) {
        return fieldsOf(ctx, {{"uri", JS_NewStringLen(ctx, uri.data(), uri.size())}});
    }

    JSValue validateUri(JSContext *ctx, const ScriptNetModuleConfig &config, const std::string &text, bool webSocket) {
        const std::string secureScheme = webSocket ? "wss" : "https";
        const std::string plainScheme = webSocket ? "ws" : "http";
        switch (ScriptNetBinding::checkUri(config, text, webSocket)) {
            case ScriptNetBinding::UriCheck::Allowed:
                return JS_UNDEFINED;
            case ScriptNetBinding::UriCheck::TlsOnly:
                return refusal(ctx, "TLSOnlyError",
                               "HTTPS is required for HTTP requests and WSS is required for Web Sockets.",
                               uriFields(ctx, text));
            case ScriptNetBinding::UriCheck::BadScheme:
                return refusal(ctx, "MalformedUriError",
                               "Supported URI schemes are '" + secureScheme + "' and '" + plainScheme
                                       + "', the value '" + NetUri::split(text).mScheme + "' provided is invalid.");
            case ScriptNetBinding::UriCheck::NoHost:
                return refusal(ctx, "MalformedUriError", "No valid hostname was found in provided URI '" + text + "'.");
            case ScriptNetBinding::UriCheck::NotAllowed:
                break;
        }
        return refusal(ctx, "UriNotAllowedError", "Request URI is not permitted by configuration.",
                       uriFields(ctx, text));
    }

    bool hasHeader(const HttpsClient::Headers &headers, const std::string &name) {
        for (const auto &header: headers) {
            if (iequals(header.first, name))
                return true;
        }
        return false;
    }

    // sub_14B8159D0 (26.60): the session headers first, then the script's headers whose name is still free
    HttpsClient::Headers mergeHeaders(ScriptApi &api, JSContext *ctx, JSValueConst list) {
        HttpsClient::Headers headers;
        for (const auto &header: api.netConfig().mSessionHeaders) {
            if (!hasHeader(headers, header.first))
                headers.push_back(header);
        }
        const ScriptPackConfig &config = api.host().getScriptEngine().packConfigs().get(api.scriptModuleUuid());
        uint32_t length = 0;
        JSValue lengthValue = JS_GetPropertyStr(ctx, list, "length");
        JS_ToUint32(ctx, &length, lengthValue);
        JS_FreeValue(ctx, lengthValue);
        for (uint32_t index = 0; index < length; ++index) {
            JSValue pair = JS_GetPropertyUint32(ctx, list, index);
            JSValue key = JS_GetPropertyUint32(ctx, pair, 0);
            JSValue value = JS_GetPropertyUint32(ctx, pair, 1);
            const std::string name = stringOf(ctx, key);
            std::string text;
            if (JS_IsString(value)) {
                text = stringOf(ctx, value);
            } else {
                // a SecretString: a secrets.json name resolved here, or the script's own value
                JSValue secretName = JS_GetPropertyStr(ctx, value, "name");
                JSValue own = JS_GetPropertyStr(ctx, value, "value");
                if (JS_IsString(secretName)) {
                    const std::string *secret = config.findSecret(stringOf(ctx, secretName));
                    if (secret != nullptr)
                        text = *secret;
                } else if (JS_IsString(own)) {
                    text = stringOf(ctx, own);
                }
                JS_FreeValue(ctx, secretName);
                JS_FreeValue(ctx, own);
            }
            if (!hasHeader(headers, name))
                headers.emplace_back(name, text);
            JS_FreeValue(ctx, key);
            JS_FreeValue(ctx, value);
            JS_FreeValue(ctx, pair);
        }
        return headers;
    }

    std::string wireMethod(const std::string &method) {
        std::string upper = method;
        for (char &c: upper)
            c = (char) std::toupper((unsigned char) c);
        return upper;
    }

    JSValue bodyTooLarge(JSContext *ctx, uint64_t maxBytes, uint64_t provided) {
        return refusal(ctx, "RequestBodyTooLargeError", BODY_TOO_LARGE,
                       fieldsOf(ctx, {{"maxBytes", JS_NewFloat64(ctx, (double) maxBytes)},
                                      {"providedBytes", JS_NewFloat64(ctx, (double) provided)}}));
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
        const std::string member = argc >= 1 ? stringOf(ctx, argv[0]) : std::string();
        const ScriptSurface &surface = apiOf(ctx).netSurface();
        // defined, not set: a setter a pack put on Array.prototype must not see or replace them
        JSValue array = JS_NewArray(ctx);
        JS_DefinePropertyValueUint32(ctx, array, 0, JS_NewBool(ctx, surface.isEarly(member)), JS_PROP_C_W_E);
        JS_DefinePropertyValueUint32(ctx, array, 1, JS_NewBool(ctx, surface.isBlocked(member)), JS_PROP_C_W_E);
        return array;
    }

    JSValue enumValues(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        const std::string name = argc >= 1 ? stringOf(ctx, argv[0]) : std::string();
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const auto &value: apiOf(ctx).netSurface().enumValues(name)) {
            JSValue row = JS_NewArray(ctx);
            JS_SetPropertyUint32(ctx, row, 0, JS_NewString(ctx, value.first.c_str()));
            const bool numeric = !value.second.empty()
                                 && value.second.find_first_not_of("0123456789") == std::string::npos;
            JS_SetPropertyUint32(ctx, row, 1,
                                 numeric ? JS_NewInt32(ctx, std::atoi(value.second.c_str()))
                                         : JS_NewString(ctx, value.second.c_str()));
            JS_SetPropertyUint32(ctx, array, index++, row);
        }
        return array;
    }

    JSValue isSerializable(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        return JS_NewBool(ctx, argc >= 1 && apiOf(ctx).native<ScriptStructureRef>(argv[0]) != nullptr);
    }

    // order of ScriptNetHttpClient::request (sub_14B819F70) and _sendRequest (sub_14B81D770), 26.60
    JSValue request(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi &api = apiOf(ctx);
        if (argc < 6)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        uint32_t id = 0;
        JS_ToUint32(ctx, &id, argv[0]);
        const std::string uri = stringOf(ctx, argv[1]);
        const ScriptNetModuleConfig &config = api.netConfig();

        JSValue invalid = validateUri(ctx, config, uri, false);
        if (!JS_IsUndefined(invalid))
            return invalid;

        ScriptNetService &state = api.netService();
        if (config.mMaxConcurrentRequests.has_value() && state.mPending.size() >= *config.mMaxConcurrentRequests)
            return refusal(ctx, "HttpRequestLimitExceededError", "Maximum concurrent HTTP requests exceeded.",
                           fieldsOf(ctx, {{"inFlightRequests", JS_NewFloat64(ctx, (double) state.mPending.size())},
                                          {"maxConcurrentRequests",
                                           JS_NewFloat64(ctx, (double) *config.mMaxConcurrentRequests)}}));

        HttpsClient::Request outgoing;
        outgoing.mUrl = uri;
        // read on a worker outside the script heap: a body it could never hold is refused there
        outgoing.mMaxResponseBytes = ScriptEngine::SCRIPT_MEMORY_LIMIT_BYTES;
        outgoing.mMethod = wireMethod(stringOf(ctx, argv[2]));
        outgoing.mHeaders = mergeHeaders(api, ctx, argv[3]);
        double timeout = 0.0;
        JS_ToFloat64(ctx, &timeout, argv[5]);
        outgoing.mTimeoutSeconds = std::isfinite(timeout) && timeout > 0.0 ? (uint32_t) std::min(timeout, 4294967295.0)
                                                                           : 0;

        const ScriptStructureRef *structure = api.native<ScriptStructureRef>(argv[4]);
        if (structure != nullptr) {
            std::shared_ptr<McStructure> found = api.host().getScriptEngine().structures().find(structure->mId);
            std::string encoded;
            if (found == nullptr || !Gzip::compress(found->serialize(), encoded))
                return refusal(ctx, "SerializableParseError", "Failed to serialize the provided body. Content-Type: ");
            outgoing.mBody = std::move(encoded);
        } else {
            outgoing.mBody = stringOf(ctx, argv[4]);
        }

        if (config.mMaxBodyBytes.has_value() && outgoing.mBody.size() > *config.mMaxBodyBytes)
            return bodyTooLarge(ctx, *config.mMaxBodyBytes, outgoing.mBody.size());
        if (structure != nullptr) {
            if (hasHeader(outgoing.mHeaders, "Content-Type") || hasHeader(outgoing.mHeaders, "Content-Encoding"))
                return refusal(ctx, "SerializableParseError",
                               std::string("Content-Type and Content-Encoding headers must not be set when the request "
                                           "body is a serializable object. They are determined automatically by the "
                                           "serializer (")
                                       + STRUCTURE_CONTENT_TYPE + ").");
            outgoing.mHeaders.emplace_back("Content-Type", STRUCTURE_CONTENT_TYPE);
            outgoing.mHeaders.emplace_back("Content-Encoding", "gzip");
        }

        state.mPending.insert(id);
        api.netService().startRequest(id, std::move(outgoing));
        return JS_UNDEFINED;
    }

    JSValue cancelAll(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        apiOf(ctx).netService().mPending.clear();
        return JS_UNDEFINED;
    }

    JSValue structureFailure(JSContext *ctx, const std::string &message) {
        return refusal(ctx, "SerializableParseError", message);
    }

    // HttpResponse::deserialize (sub_14B8AB640) and the structure deserializer (sub_14DE13650), 26.60
    JSValue deserialize(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi &api = apiOf(ctx);
        const ScriptNetBody *body = argc >= 1 ? api.native<ScriptNetBody>(argv[0]) : nullptr;
        if (body == nullptr || body->mBytes.empty())
            return structureFailure(ctx, "Response body is empty or deserialization support is not available.");
        if (normalizeContentType(body->mContentType) != normalizeContentType(STRUCTURE_CONTENT_TYPE))
            return structureFailure(ctx, "No deserializer registered for Content-Type: " + body->mContentType + ".");

        const std::string raw = argc >= 2 ? stringOf(ctx, argv[1]) : std::string();
        if (!ScriptStructureRegistry::isValidIdentifier(raw))
            return structureFailure(ctx, "'" + raw + "' is not a valid structure identifier.");
        ScriptStructureRegistry &registry = api.host().getScriptEngine().structures();
        const std::string id = ScriptStructureRegistry::normalize(raw);
        if (registry.exists(id))
            return structureFailure(ctx, "A structure named '" + raw + "' already exists.");

        Tag root;
        try {
            ReadOnlyBinaryStream stream(body->mBytes);
            root = NbtIo::readTag(stream, NbtVariant::LittleEndian);
        } catch (const std::exception &) {
            return structureFailure(ctx, "Response body could not be parsed as structure data.");
        }
        auto structure = std::make_shared<McStructure>();
        if (!McStructure::fromNbt(root, *structure) || !registry.add(id, structure, false))
            return structureFailure(ctx, "Structure data was parsed but could not be loaded.");
        return api.makeStructure(id);
    }

    JSValue wsConnect(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi &api = apiOf(ctx);
        if (argc < 3)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        uint32_t id = 0;
        JS_ToUint32(ctx, &id, argv[0]);
        const std::string uri = stringOf(ctx, argv[1]);
        const ScriptNetModuleConfig &config = api.netConfig();
        ScriptNetService &service = api.netService();

        // sub_14DE1ACA0 (26.60): the socket limit comes before the uri check
        if (config.mMaxWebsocketConnections.has_value() && service.sockets().size() >= *config.mMaxWebsocketConnections)
            return refusal(ctx, "WebSocketLimitExceededError", "Maximum concurrent web socket connections exceeded.",
                           fieldsOf(ctx, {{"connectedSockets", JS_NewFloat64(ctx, (double) service.sockets().size())},
                                          {"maxConcurrentConnections",
                                           JS_NewFloat64(ctx, (double) *config.mMaxWebsocketConnections)}}));
        JSValue invalid = validateUri(ctx, config, uri, true);
        if (!JS_IsUndefined(invalid))
            return invalid;

        // a message past the per tick allowance closes the socket anyway; none can outgrow the script heap
        uint64_t maxMessage = ScriptEngine::SCRIPT_MEMORY_LIMIT_BYTES;
        if (config.mMaxWebsocketPayloadSize.has_value())
            maxMessage = std::min<uint64_t>(maxMessage, *config.mMaxWebsocketPayloadSize);
        service.openSocket(id, uri, mergeHeaders(api, ctx, argv[2]), maxMessage);
        service.mSocketUris[id] = uri;
        return JS_UNDEFINED;
    }

    JSValue wsSend(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi &api = apiOf(ctx);
        uint32_t id = 0;
        if (argc < 2 || JS_ToUint32(ctx, &id, argv[0]) != 0)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const std::string payload = stringOf(ctx, argv[1]);
        const ScriptNetModuleConfig &config = api.netConfig();
        if (config.mMaxBodyBytes.has_value() && payload.size() > *config.mMaxBodyBytes)
            return bodyTooLarge(ctx, *config.mMaxBodyBytes, payload.size());
        WebSocketClient *socket = api.netService().findSocket(id);
        if (socket == nullptr)
            return refusal(ctx, "WebSocketNotConnectedError", "This web socket is not connected.");
        socket->send(payload);
        return JS_UNDEFINED;
    }

    JSValue wsClose(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        uint32_t id = 0;
        if (argc >= 1)
            JS_ToUint32(ctx, &id, argv[0]);
        WebSocketClient *socket = apiOf(ctx).netService().findSocket(id);
        if (socket != nullptr)
            socket->close();
        return JS_UNDEFINED;
    }

    JSValue packetInterest(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        if (argc < 2)
            return JS_UNDEFINED;
        ScriptApi &api = apiOf(ctx);
        api.setPacketInterest(JS_ToBool(ctx, argv[0]) == 1, JS_ToBool(ctx, argv[1]) == 1);
        api.host().getScriptEngine().updatePacketHooks();
        return JS_UNDEFINED;
    }

    JSValue report(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        if (argc >= 1) {
            JS_Throw(ctx, JS_DupValue(ctx, argv[0]));
            apiOf(ctx).logPendingException("event handler (@minecraft/server-net)");
        }
        return JS_UNDEFINED;
    }

    void add(JSContext *ctx, JSValue object, const char *name, JSCFunction *function, int length) {
        JS_SetPropertyStr(ctx, object, name, JS_NewCFunction(ctx, function, name, length));
    }

    // a hook of the bootstrap with its arguments, which it frees
    bool callHook(ScriptApi &api, const char *name, std::initializer_list<JSValue> arguments) {
        JSContext *ctx = api.context();
        std::vector<JSValue> args(arguments);
        JSValue hook = JS_GetPropertyStr(ctx, api.netHooks(), name);
        JSValue result = JS_Call(ctx, hook, api.netHooks(), (int) args.size(), args.data());
        for (JSValue arg: args)
            JS_FreeValue(ctx, arg);
        JS_FreeValue(ctx, hook);
        if (JS_IsException(result)) {
            api.logPendingException(name);
            return false;
        }
        const bool value = JS_ToBool(ctx, result) == 1;
        JS_FreeValue(ctx, result);
        return value;
    }

    JSValue string(JSContext *ctx, const std::string &text) {
        return JS_NewStringLen(ctx, text.data(), text.size());
    }
}

namespace ScriptNetBinding {
    // ScriptNetHttpClient uri check (sub_14B81B7C0, 26.60)
    UriCheck checkUri(const ScriptNetModuleConfig &config, const std::string &text, bool webSocket) {
        const NetUri uri = NetUri::split(text);
        const bool secure = iequals(uri.mScheme, webSocket ? "wss" : "https");
        const bool plain = iequals(uri.mScheme, webSocket ? "ws" : "http");
        // only the presence of force_tls is read, its value is not
        if (!secure && config.mForceTls.has_value())
            return UriCheck::TlsOnly;
        if (!secure && !plain)
            return UriCheck::BadScheme;
        if (uri.mAuthority.empty())
            return UriCheck::NoHost;
        if (!config.mAllowedUris.has_value())
            return UriCheck::Allowed;
        for (const std::string &entry: *config.mAllowedUris) {
            const NetUri allowed = NetUri::split(entry);
            if (allowed.mAuthority.empty())
                continue;
            if (!allowed.mScheme.empty() && !iequals(allowed.mScheme, uri.mScheme))
                continue;
            if (!iequals(allowed.mAuthority, uri.mAuthority))
                continue;
            if (!allowed.mPath.empty() && uri.mPath.compare(0, allowed.mPath.size(), allowed.mPath) != 0)
                continue;
            return UriCheck::Allowed;
        }
        return UriCheck::NotAllowed;
    }

    const std::vector<const char *> &exportNames() {
        static const std::vector<const char *> names = {"CloseAfterEventSignal",
                                                        "HttpClient",
                                                        "HttpHeader",
                                                        "HttpRequest",
                                                        "HttpRequestMethod",
                                                        "HttpResponse",
                                                        "HttpStatusCode",
                                                        "MessageAfterEventSignal",
                                                        "NetworkBeforeEvents",
                                                        "PacketId",
                                                        "PacketReceiveBeforeEventSignal",
                                                        "PacketReceivedBeforeEvent",
                                                        "PacketSendBeforeEvent",
                                                        "PacketSendBeforeEventSignal",
                                                        "WebSocket",
                                                        "WebSocketClient",
                                                        "WebSocketClientAfterEvents",
                                                        "WebSocketClientCloseAfterEvent",
                                                        "WebSocketClientCloseReasons",
                                                        "WebSocketClientReceiveAfterEvent",
                                                        "beforeEvents",
                                                        "http",
                                                        "websocket"};
        return names;
    }

    const char *packetName(MinecraftPacketIds id) {
        static const std::unordered_map<int, const char *> names = [] {
            std::unordered_map<int, const char *> built;
            for (const PacketName &entry: PACKET_NAMES)
                built.emplace((int) entry.mId, entry.mName);
            return built;
        }();
        const auto it = names.find((int) id);
        return it == names.end() ? nullptr : it->second;
    }

    JSValue install(ScriptApi &api, JSValueConst secretOf) {
        JSContext *ctx = api.context();
        JSValue native = JS_NewObject(ctx);
        add(ctx, native, "privilege", privilege, 0);
        add(ctx, native, "flags", flags, 1);
        add(ctx, native, "enumValues", enumValues, 1);
        add(ctx, native, "isSerializable", isSerializable, 1);
        add(ctx, native, "request", request, 6);
        add(ctx, native, "cancelAll", cancelAll, 0);
        add(ctx, native, "deserialize", deserialize, 2);
        add(ctx, native, "wsConnect", wsConnect, 3);
        add(ctx, native, "wsSend", wsSend, 2);
        add(ctx, native, "wsClose", wsClose, 1);
        add(ctx, native, "packetInterest", packetInterest, 2);
        add(ctx, native, "report", report, 1);
        JS_SetPropertyStr(ctx, native, "errors", JS_DupValue(ctx, api.errors()));

        JSValue factory = JS_Eval(ctx, NET, std::strlen(NET), "<net>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(factory)) {
            JS_FreeValue(ctx, native);
            api.logPendingException("<net>");
            return JS_NewObject(ctx);
        }
        JSValue args[2] = {native, JS_DupValue(ctx, secretOf)};
        JSValue result = JS_Call(ctx, factory, JS_UNDEFINED, 2, args);
        JS_FreeValue(ctx, factory);
        JS_FreeValue(ctx, args[0]);
        JS_FreeValue(ctx, args[1]);
        if (JS_IsException(result)) {
            api.logPendingException("<net>");
            return JS_NewObject(ctx);
        }
        return result;
    }

    void tick(ScriptApi &api) {
        if (!JS_IsObject(api.netHooks()))
            return;
        JSContext *ctx = api.context();
        ScriptNetService &service = api.netService();
        ScriptNetService &state = service;

        for (ScriptNetService::Completed &done: service.pollRequests()) {
            if (state.mPending.erase(done.mId) == 0)
                continue;
            HttpsClient::Response &response = done.mResponse;
            if (!response.mCompleted) {
                const std::string message = "Internal HTTP error occurred with message '" + response.mErrorMessage
                                            + "' and code " + std::to_string(response.mErrorCode) + ".";
                callHook(api, "httpFail",
                         {JS_NewUint32(ctx, done.mId), JS_NewString(ctx, "InternalHttpRequestError"),
                          string(ctx, message),
                          fieldsOf(ctx, {{"errorCode", JS_NewInt32(ctx, response.mErrorCode)},
                                         {"errorMessage", string(ctx, response.mErrorMessage)}})});
                continue;
            }
            JSValue headers = JS_NewArray(ctx);
            uint32_t index = 0;
            ScriptNetBody *body = new ScriptNetBody();
            for (const auto &header: response.mHeaders) {
                if (body->mContentType.empty() && iequals(header.first, "Content-Type"))
                    body->mContentType = header.second;
                JSValue row = JS_NewArray(ctx);
                JS_SetPropertyUint32(ctx, row, 0, string(ctx, header.first));
                JS_SetPropertyUint32(ctx, row, 1, string(ctx, header.second));
                JS_SetPropertyUint32(ctx, headers, index++, row);
            }
            JSValue text = string(ctx, response.mBody);
            body->mBytes = std::move(response.mBody);
            callHook(api, "httpDone",
                     {JS_NewUint32(ctx, done.mId), JS_NewInt32(ctx, response.mStatus), headers, text,
                      api.makeOpaque(body)});
        }

        std::vector<uint32_t> finished;
        for (const auto &entry: service.sockets()) {
            const uint32_t id = entry.first;
            uint64_t &received = state.mReceived[id];
            received = 0;
            bool opened = false;
            std::deque<WebSocketClient::Event> &backlog = service.mBacklog[id];
            for (WebSocketClient::Event &event: entry.second->poll())
                backlog.push_back(std::move(event));
            while (!backlog.empty()) {
                WebSocketClient::Event event = std::move(backlog.front());
                backlog.pop_front();
                switch (event.mKind) {
                    case WebSocketClient::Event::Kind::Opened:
                        callHook(api, "wsOpened", {JS_NewUint32(ctx, id)});
                        // the rest waits a tick: the connect promise settles before the script can subscribe
                        opened = true;
                        break;
                    case WebSocketClient::Event::Kind::Failed:
                        finished.push_back(id);
                        if (event.mStatus != 0) {
                            callHook(api, "wsFailed",
                                     {JS_NewUint32(ctx, id), JS_NewString(ctx, "WebSocketConnectionFailedError"),
                                      JS_NewString(ctx, "Failed to connect web socket with uri"),
                                      fieldsOf(ctx, {{"errorCode", JS_NewInt32(ctx, event.mStatus)},
                                                     {"uri", string(ctx, service.mSocketUris[id])}})});
                        } else {
                            const std::string message = "Internal Web Socket error occurred with message '"
                                                        + event.mText + "' and code " + std::to_string(event.mErrorCode)
                                                        + ".";
                            callHook(api, "wsFailed",
                                     {JS_NewUint32(ctx, id), JS_NewString(ctx, "InternalWebSocketError"),
                                      string(ctx, message),
                                      fieldsOf(ctx, {{"errorCode", JS_NewInt32(ctx, event.mErrorCode)},
                                                     {"errorMessage", string(ctx, event.mText)}})});
                        }
                        break;
                    case WebSocketClient::Event::Kind::Message: {
                        received += event.mText.size();
                        const ScriptNetModuleConfig &config = api.netConfig();
                        if (config.mMaxWebsocketPayloadSize.has_value()
                            && received > *config.mMaxWebsocketPayloadSize) {
                            // WebSocketClientCloseReasons.IncomingPayloadsTooLarge: over the per tick allowance
                            entry.second->close();
                            finished.push_back(id);
                            callHook(api, "wsClosed", {JS_NewUint32(ctx, id), JS_NewInt32(ctx, 2)});
                            break;
                        }
                        callHook(api, "wsMessage", {JS_NewUint32(ctx, id), string(ctx, event.mText)});
                        break;
                    }
                    case WebSocketClient::Event::Kind::Closed: {
                        finished.push_back(id);
                        // 2: WebSocketClientCloseReasons.IncomingPayloadsTooLarge, as for the per tick allowance
                        const int32_t reason = event.mTooLarge ? 2 : event.mByServer ? 0 : 1;
                        callHook(api, "wsClosed", {JS_NewUint32(ctx, id), JS_NewInt32(ctx, reason)});
                        break;
                    }
                }
                if (opened || (!finished.empty() && finished.back() == id))
                    break;
            }
        }
        for (uint32_t id: finished)
            service.removeSocket(id);
    }
}
