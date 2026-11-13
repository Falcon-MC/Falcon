#include "Scripting/Binding/ScriptApi.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/Net/HttpsClient.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/Binding/ScriptNetBinding.h"
#include "Scripting/Net/ScriptNetService.h"
#include "Scripting/ScriptEngine.h"
#include "Server/ServerHost.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <optional>
#include <random>
#include <regex>
#include <string>

namespace {
    // ScriptModuleDiagnostics::ScriptSentry (26.60): the client behind sentry, its envelope built like
    // SentryEnvelopeHeader / SentryEnvelopeItem / SentryEnvelopePayload (reflection names, sub_143007970)
    const char *SOURCE = R"JS(
(function (native) {
    const token = Symbol();
    const conversion = () => new TypeError("Native type conversion failed.");
    const guard = (label, cls, member) => {
        const flags = native.flags(cls + "." + member);
        const privilege = native.privilege();
        if (privilege === 1 && !flags.early)
            throw new ReferenceError(label + " [" + cls + "::" + member + "] cannot be used in early execution.");
        if (privilege === 2 && flags.blocked)
            throw new ReferenceError(label + " [" + cls + "::" + member + "] cannot be used in restricted execution.");
    };
    const string = (value) => {
        if (typeof value !== "string")
            throw conversion();
        return value;
    };
    const named = (cls, name) => Object.defineProperty(cls, "name", { value: name });
    const LEVELS = ["debug", "error", "fatal", "info", "warning"];
    const level = (value) => {
        if (LEVELS.indexOf(value) < 0)
            throw conversion();
        return value;
    };
    // sub_14BA24690, sub_14BA24510
    const uninitialized = () => native.errors.make("SentryUninitializedError", "Sentry is not initialized");
    const alreadyInitialized = () => native.errors.make("SentryAlreadyInitializedError",
        "Sentry is already initialized");
    // sub_148549BD0 with InvalidArgumentErrorType.Unspecified and no argument index
    const invalidArgument = (text) => {
        const error = new native.errors.classes.InvalidArgumentError(undefined, undefined, "Unspecified", undefined);
        Object.defineProperty(error, "message", { value: "Invalid value passed. " + text, writable: true,
            configurable: true });
        return error;
    };

    let client;
    const Sentry = class {
        constructor(key) {
            if (key !== token)
                throw new ReferenceError("No constructor for native class 'Sentry'.");
        }
    };
    named(Sentry, "Sentry");
    const method = (member, run) => Object.defineProperty(Sentry.prototype, member, {
        configurable: true, writable: true, enumerable: false,
        value: function (...args) {
            guard("Native function", "Sentry", member);
            return run(...args);
        },
    });
    const initialized = () => {
        if (client === undefined)
            throw uninitialized();
        return client;
    };

    method("init", (options) => {
        if (options === null || typeof options !== "object")
            throw conversion();
        if (client !== undefined)
            throw alreadyInitialized();
        const configured = native.configure(options.dsn);
        if (configured === 1)
            throw conversion();
        if (configured === 2)
            throw invalidArgument("DSN format is invalid, see Sentry documentation for correct DSN format.");
        if (configured === 3)
            throw invalidArgument("DSN must use TLS when force_tls is enabled.");
        const debug = options.debug === undefined ? false : !!options.debug;
        let maxBreadcrumbs = 20;
        if (options.maxBreadcrumbs !== undefined) {
            if (typeof options.maxBreadcrumbs !== "number")
                throw conversion();
            maxBreadcrumbs = Math.min(Math.max(Math.trunc(options.maxBreadcrumbs), 0), 100);
        }
        let sampleRate = 1;
        if (options.sampleRate !== undefined) {
            if (typeof options.sampleRate !== "number")
                throw conversion();
            sampleRate = options.sampleRate;
        }
        const info = native.info();
        client = {
            debug, maxBreadcrumbs, sampleRate, breadcrumbs: [], sessionId: native.uuid(), info,
            // the game's defaults: 10 events a 60 second window (sub_14BA26740)
            maxEvents: 10, window: 60, windowStart: undefined, sent: 0,
            tags: new Map([["minecraft_version", info.version], ["minecraft_edition", info.edition],
                ["module_uuid", info.moduleUuid], ["module_name", info.moduleName],
                ["module_runtime_id", info.runtimeId]]),
        };
        client.tags.set("minecraft_session_id", client.sessionId);
    });
    method("addTag", (name, value) => {
        const c = initialized();
        c.tags.set(string(name), string(value));
    });
    method("removeTag", (name) => {
        const c = initialized();
        c.tags.delete(string(name));
    });
    method("getTags", () => {
        const c = initialized();
        const tags = {};
        for (const [key, value] of c.tags)
            tags[key] = value;
        return tags;
    });
    // sub_14BAC08E0: the oldest breadcrumb goes once the limit is reached, none kept with a limit of 0
    method("addBreadcrumb", (crumbLevel, message, category) => {
        const c = initialized();
        const entry = { level: level(crumbLevel), message: string(message),
            category: category === undefined ? undefined : string(category), timestamp: native.time() };
        if (c.maxBreadcrumbs === 0)
            return;
        if (c.breadcrumbs.length >= c.maxBreadcrumbs)
            c.breadcrumbs.shift();
        c.breadcrumbs.push(entry);
    });

    // sub_14BA26740: sample rate, then at most maxEvents a window
    const skipped = (c) => {
        if (c.maxEvents === 0) {
            if (c.debug)
                native.log("Skipping Sentry report because max events per window is set to 0");
            return true;
        }
        if (native.random() > c.sampleRate) {
            if (c.debug)
                native.log("Skipping Sentry report due to sample rate configuration");
            return true;
        }
        const now = native.now();
        if (c.windowStart === undefined || now - c.windowStart >= c.window * 1000) {
            c.windowStart = now;
            c.sent = 0;
        }
        if (c.sent >= c.maxEvents) {
            if (c.debug)
                native.log("Skipping Sentry report due to rate limiting");
            return true;
        }
        return false;
    };
    // the script stack's frames, innermost first as the engine prints them
    const framesOf = (stack) => {
        const frames = [];
        if (typeof stack !== "string")
            return frames;
        for (const line of stack.split("\n")) {
            const match = /^\s*at (?:(.*?) \()?(.*?):(\d+)(?::\d+)?\)?$/.exec(line);
            if (match !== null)
                frames.push({ function: match[1] === undefined ? "" : match[1], filename: match[2],
                    lineno: Number(match[3]) });
        }
        return frames;
    };
    method("captureException", (exception, captureContext) => {
        const c = initialized();
        let context = {};
        if (captureContext !== undefined) {
            if (captureContext === null || typeof captureContext !== "object")
                throw conversion();
            context = captureContext;
        }
        if (skipped(c))
            return;
        const isError = exception instanceof Error;
        const type = isError ? String(exception.name) : typeof exception;
        let value;
        try {
            value = isError ? String(exception.message) : JSON.stringify(exception);
        } catch (e) {
            value = String(exception);
        }
        const mechanism = { type: "captured_exception", handled: true };
        if (context.extraData !== undefined)
            mechanism.data = context.extraData;
        const tags = {};
        for (const [key, tag] of c.tags)
            tags[key] = tag;
        if (context.tags !== undefined)
            for (const key of Object.keys(context.tags))
                tags[key] = string(context.tags[key]);
        const payload = {
            timestamp: native.time(), platform: "javascript",
            level: context.level === undefined ? "error" : level(context.level), release: c.info.release, tags,
            exception: { values: [{ type, value: value === undefined ? "" : value, mechanism }] },
            stacktrace: { frames: isError ? framesOf(exception.stack) : [] },
            breadcrumbs: { values: c.breadcrumbs.map((b) => {
                const crumb = {};
                if (b.category !== undefined)
                    crumb.category = b.category;
                crumb.level = b.level;
                crumb.message = b.message;
                crumb.timestamp = b.timestamp;
                return crumb;
            }) },
            user: { ip_address: "0.0.0.0" },
            sdk: { name: "mojang.minecraft.javascript", version: c.info.version },
        };
        native.send(JSON.stringify({ type: "event" }) + "\n" + JSON.stringify(payload));
        c.sent++;
        if (c.debug)
            native.log("Submitted error to Sentry");
    });

    const sentry = new Sentry(token);
    const SentryEventLevel = {};
    for (const name of LEVELS)
        Object.defineProperty(SentryEventLevel, name, { value: name, enumerable: true });
    return { Sentry, SentryEventLevel, sentry };
})
)JS";

    JSValue diagPrivilege(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return JS_NewInt32(ctx, (int32_t) ScriptApi::fromContext(ctx)->privilege());
    }

    JSValue diagFlags(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const char *name = argc >= 1 ? JS_ToCString(ctx, argv[0]) : nullptr;
        const std::string member = name != nullptr ? name : "";
        if (name != nullptr)
            JS_FreeCString(ctx, name);
        JSValue flags = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, flags, "early", JS_NewBool(ctx, api->diagnosticsSurface().isEarly(member)));
        JS_SetPropertyStr(ctx, flags, "blocked", JS_NewBool(ctx, api->diagnosticsSurface().isBlocked(member)));
        return flags;
    }

    // the "{:08x}-{:04x}-{:04x}-{:04x}-{:08x}{:04x}" event and session ids (sub_14BA294A0)
    std::string newUuid() {
        static std::mt19937_64 random(std::random_device{}());
        const uint64_t high = random();
        const uint64_t low = random();
        char text[40];
        std::snprintf(text, sizeof(text), "%08x-%04x-%04x-%04x-%08x%04x", (unsigned) (high >> 32),
                      (unsigned) ((high >> 16) & 0xFFFF), (unsigned) (high & 0xFFFF), (unsigned) (low >> 48),
                      (unsigned) ((low >> 16) & 0xFFFFFFFF), (unsigned) (low & 0xFFFF));
        return text;
    }

    JSValue diagUuid(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return JS_NewString(ctx, newUuid().c_str());
    }

    // escaped as JSON.stringify writes a string
    std::string jsonString(const std::string &value) {
        std::string out = "\"";
        for (const char character: value) {
            const unsigned char byte = (unsigned char) character;
            if (character == '"' || character == '\\') {
                out.push_back('\\');
                out.push_back(character);
            } else if (character == '\b') {
                out += "\\b";
            } else if (character == '\f') {
                out += "\\f";
            } else if (character == '\n') {
                out += "\\n";
            } else if (character == '\r') {
                out += "\\r";
            } else if (character == '\t') {
                out += "\\t";
            } else if (byte < 0x20) {
                char escaped[8];
                std::snprintf(escaped, sizeof(escaped), "\\u%04x", (unsigned) byte);
                out += escaped;
            } else {
                out.push_back(character);
            }
        }
        out.push_back('"');
        return out;
    }

    // _time64: whole seconds
    JSValue diagTime(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return JS_NewFloat64(ctx, (double) std::time(nullptr));
    }

    JSValue diagNow(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        using namespace std::chrono;
        return JS_NewFloat64(ctx, (double) duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
    }

    JSValue diagRandom(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        static std::mt19937 random(std::random_device{}());
        return JS_NewFloat64(ctx, std::uniform_real_distribution<double>(0.0, 1.0)(random));
    }

    JSValue diagLog(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        const char *text = argc >= 1 ? JS_ToCString(ctx, argv[0]) : nullptr;
        if (text != nullptr) {
            LOG_INFO(LogAreaID::Script, "[%s] %s", ScriptApi::fromContext(ctx)->packName().c_str(), text);
            JS_FreeCString(ctx, text);
        }
        return JS_UNDEFINED;
    }

    // a string, or a SecretString: a secrets.json name or the script's own value; nullopt when neither
    std::optional<std::string> dsnOf(JSContext *ctx, JSValueConst value) {
        if (JS_IsString(value)) {
            const char *text = JS_ToCString(ctx, value);
            std::optional<std::string> result;
            if (text != nullptr) {
                result = std::string(text);
                JS_FreeCString(ctx, text);
            }
            return result;
        }
        if (!JS_IsObject(value))
            return std::nullopt;
        ScriptApi *api = ScriptApi::fromContext(ctx);
        JSValue secretOf = api->secretOf();
        if (!JS_IsObject(secretOf))
            return std::nullopt;
        JSValue get = JS_GetPropertyStr(ctx, secretOf, "get");
        JSValue entry = JS_Call(ctx, get, secretOf, 1, &value);
        JS_FreeValue(ctx, get);
        std::optional<std::string> result;
        if (JS_IsObject(entry)) {
            JSValue name = JS_GetPropertyStr(ctx, entry, "name");
            JSValue own = JS_GetPropertyStr(ctx, entry, "value");
            if (JS_IsString(name)) {
                const char *text = JS_ToCString(ctx, name);
                const std::string *secret = api->host()
                                                    .getScriptEngine()
                                                    .packConfigs()
                                                    .get(api->scriptModuleUuid())
                                                    .findSecret(text != nullptr ? text : "");
                if (text != nullptr)
                    JS_FreeCString(ctx, text);
                if (secret != nullptr)
                    result = *secret;
            } else if (JS_IsString(own)) {
                const char *text = JS_ToCString(ctx, own);
                if (text != nullptr) {
                    result = std::string(text);
                    JS_FreeCString(ctx, text);
                }
            }
            JS_FreeValue(ctx, name);
            JS_FreeValue(ctx, own);
        }
        JS_FreeValue(ctx, entry);
        return result;
    }

    // 0 set, 1 not a string nor a SecretString, 2 not a dsn, 3 plain http under force_tls (sub_14BABC030); parsed
    // here so a secret never reaches the script
    JSValue diagConfigure(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        const std::optional<std::string> dsn = argc >= 1 ? dsnOf(ctx, argv[0]) : std::nullopt;
        if (!dsn.has_value())
            return JS_NewInt32(ctx, 1);

        // sub_14BABC030: (https?)://(key)@(host)/(project), the envelope endpoint
        static const std::regex DSN("^(https?)://([^@]+)@([^/]+)/(\\d+)$");
        std::smatch parts;
        if (!std::regex_match(*dsn, parts, DSN))
            return JS_NewInt32(ctx, 2);

        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptNetModuleConfig config =
                api->host().getScriptEngine().packConfigs().get(api->scriptModuleUuid()).netConfig();
        // only the presence of force_tls is read, as for server-net
        if (parts[1].str() != "https" && config.mForceTls.has_value())
            return JS_NewInt32(ctx, 3);
        api->diagnosticsDsn() = *dsn;
        api->diagnosticsUrl() = parts[1].str() + "://" + parts[3].str() + "/api/" + parts[4].str() + "/envelope/";
        return JS_NewInt32(ctx, 0);
    }

    JSValue diagInfo(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const std::string version = ServerHostOptions().gameVersion;
        JSValue info = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, info, "version", JS_NewString(ctx, version.c_str()));
        JS_SetPropertyStr(ctx, info, "edition", JS_NewString(ctx, "dedicated_server"));
        JS_SetPropertyStr(ctx, info, "moduleName", JS_NewString(ctx, api->packName().c_str()));
        JS_SetPropertyStr(ctx, info, "moduleUuid", JS_NewString(ctx, api->scriptModuleUuid().c_str()));
        JS_SetPropertyStr(ctx, info, "runtimeId", JS_NewString(ctx, api->packUuid().c_str()));
        JS_SetPropertyStr(ctx, info, "release", JS_NewString(ctx, api->scriptModuleVersion().c_str()));
        return info;
    }

    // a POST nobody waits on: the answer is dropped like the game's transport drops it; held to the pack's
    // server-net rules, the only network limits a pack is given
    JSValue diagSend(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1 || api->diagnosticsUrl().empty())
            return JS_UNDEFINED;
        size_t length = 0;
        const char *items = JS_ToCStringLen(ctx, &length, argv[0]);
        if (items == nullptr)
            return JS_UNDEFINED;

        // sub_14BA294A0: a new event id and the dsn in the envelope header, then the event item
        HttpsClient::Request request;
        request.mMethod = "POST";
        request.mUrl = api->diagnosticsUrl();
        request.mBody = "{\"event_id\":" + jsonString(newUuid()) + ",\"dsn\":" + jsonString(api->diagnosticsDsn())
                        + "}\n";
        request.mBody.append(items, length);
        request.mTimeoutSeconds = HttpsClient::DEFAULT_TIMEOUT_SECONDS;
        JS_FreeCString(ctx, items);

        const ScriptNetModuleConfig config =
                api->host().getScriptEngine().packConfigs().get(api->scriptModuleUuid()).netConfig();
        if (ScriptNetBinding::checkUri(config, request.mUrl, false) != ScriptNetBinding::UriCheck::Allowed) {
            LOG_WARN(LogAreaID::Script, "[%s] Sentry report not sent: its address is not allowed by the pack's "
                     "server-net configuration", api->packName().c_str());
            return JS_UNDEFINED;
        }
        if (config.mMaxBodyBytes.has_value() && request.mBody.size() > *config.mMaxBodyBytes) {
            LOG_WARN(LogAreaID::Script, "[%s] Sentry report not sent: larger than the pack's max_body_bytes",
                     api->packName().c_str());
            return JS_UNDEFINED;
        }

        api->netService().startRequest(api->nextUntrackedRequestId(), std::move(request));
        return JS_UNDEFINED;
    }

    void addFunction(JSContext *ctx, JSValue object, const char *name, JSCFunction *function, int length) {
        JS_SetPropertyStr(ctx, object, name, JS_NewCFunction(ctx, function, name, length));
    }
}

void ScriptApi::_installDiagnostics() {
    JSValue native = JS_NewObject(mContext);
    addFunction(mContext, native, "privilege", diagPrivilege, 0);
    addFunction(mContext, native, "flags", diagFlags, 1);
    addFunction(mContext, native, "uuid", diagUuid, 0);
    addFunction(mContext, native, "time", diagTime, 0);
    addFunction(mContext, native, "now", diagNow, 0);
    addFunction(mContext, native, "random", diagRandom, 0);
    addFunction(mContext, native, "log", diagLog, 1);
    addFunction(mContext, native, "configure", diagConfigure, 1);
    addFunction(mContext, native, "info", diagInfo, 0);
    addFunction(mContext, native, "send", diagSend, 1);
    JS_SetPropertyStr(mContext, native, "errors", JS_DupValue(mContext, mErrors));

    JSValue factory = JS_Eval(mContext, SOURCE, std::strlen(SOURCE), "<diagnostics>", JS_EVAL_TYPE_GLOBAL);
    JSValue exports = factory;
    if (!JS_IsException(factory)) {
        exports = JS_Call(mContext, factory, JS_UNDEFINED, 1, &native);
        JS_FreeValue(mContext, factory);
    }
    JS_FreeValue(mContext, native);
    if (JS_IsException(exports)) {
        logPendingException("<diagnostics>");
        return;
    }
    for (const char *name: {"Sentry", "SentryEventLevel", "sentry"}) {
        if (mDiagnosticsSurface.has(name))
            mDiagnosticsExports.emplace_back(name, JS_GetPropertyStr(mContext, exports, name));
    }
    JS_FreeValue(mContext, exports);
}
