#include "Scripting/ScriptEngine.h"

#include "Core/Debug/BedrockLog.h"
#include "Scripting/Binding/ScriptApi.h"

#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <quickjs.h>

namespace {
    int64_t nowMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    std::string valueToString(JSContext *ctx, JSValueConst value) {
        const char *chars = JS_ToCString(ctx, value);
        if (chars == nullptr)
            return std::string();

        std::string result(chars);
        JS_FreeCString(ctx, chars);
        return result;
    }

    JSValue consoleLog(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        std::ostringstream line;
        for (int i = 0; i < argc; ++i) {
            if (i != 0)
                line << ' ';
            line << valueToString(ctx, argv[i]);
        }

        LOG_INFO(LogAreaID::Server, "[script] %s", line.str().c_str());
        return JS_UNDEFINED;
    }

    JSValue consoleWarn(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        std::ostringstream line;
        for (int i = 0; i < argc; ++i) {
            if (i != 0)
                line << ' ';
            line << valueToString(ctx, argv[i]);
        }

        LOG_WARN(LogAreaID::Server, "[script] %s", line.str().c_str());
        return JS_UNDEFINED;
    }
}

namespace {
    std::string normalizeModulePath(const std::string &baseName, const std::string &name) {
        if (name.empty() || (name[0] != '.'))
            return name;

        std::string directory;
        const size_t slash = baseName.find_last_of('/');
        if (slash != std::string::npos)
            directory = baseName.substr(0, slash);

        std::string combined = directory.empty() ? name : directory + "/" + name;

        std::vector<std::string> segments;
        size_t start = 0;
        while (start <= combined.size()) {
            size_t end = combined.find('/', start);
            if (end == std::string::npos)
                end = combined.size();

            const std::string segment = combined.substr(start, end - start);
            if (segment == "..") {
                if (!segments.empty())
                    segments.pop_back();
            } else if (segment != "." && !segment.empty()) {
                segments.push_back(segment);
            }
            start = end + 1;
        }

        std::string result;
        for (size_t i = 0; i < segments.size(); ++i) {
            if (i != 0)
                result += "/";
            result += segments[i];
        }
        return result;
    }

    std::string readModuleFile(const std::string &name) {
        std::vector<std::string> candidates = {name};
        if (name.size() < 3 || name.substr(name.size() - 3) != ".js")
            candidates.push_back(name + ".js");

        for (const std::string &candidate: candidates) {
            std::ifstream file(candidate.c_str(), std::ios::binary);
            if (!file.is_open())
                continue;

            std::ostringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        }
        return std::string();
    }

    char *scriptModuleNormalize(JSContext *ctx, const char *baseName, const char *name, void *) {
        const std::string normalized = normalizeModulePath(baseName, name);
        const size_t length = normalized.size();
        char *result = (char *) js_malloc(ctx, length + 1);
        if (result == nullptr)
            return nullptr;
        memcpy(result, normalized.data(), length);
        result[length] = '\0';
        return result;
    }

    JSModuleDef *scriptModuleLoader(JSContext *ctx, const char *moduleName, void *) {
        const std::string source = readModuleFile(moduleName);
        if (source.empty()) {
            JS_ThrowReferenceError(ctx, "could not load module '%s'", moduleName);
            return nullptr;
        }

        JSValue compiled = JS_Eval(ctx, source.c_str(), source.size(), moduleName,
                                   JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
        if (JS_IsException(compiled))
            return nullptr;

        JSModuleDef *module = (JSModuleDef *) JS_VALUE_GET_PTR(compiled);
        JS_FreeValue(ctx, compiled);
        return module;
    }
}

int _scriptInterruptHandler(JSRuntime *, void *opaque) {
    const ScriptEngine *engine = (const ScriptEngine *) opaque;
    if (engine == nullptr)
        return 0;

    return nowMs() > engine->mExecutionDeadlineMs ? 1 : 0;
}

ScriptEngine::ScriptEngine()
        : mRuntime(nullptr), mContext(nullptr), mWatchdogMs(DEFAULT_WATCHDOG_MS), mExecutionDeadlineMs(0) {
    mRuntime = JS_NewRuntime();
    if (mRuntime == nullptr) {
        LOG_ERROR(LogAreaID::Server, "Failed to create the QuickJS runtime");
        return;
    }

    JS_SetInterruptHandler(mRuntime, _scriptInterruptHandler, this);
    JS_SetModuleLoaderFunc(mRuntime, scriptModuleNormalize, scriptModuleLoader, nullptr);

    mContext = JS_NewContext(mRuntime);
    if (mContext == nullptr) {
        LOG_ERROR(LogAreaID::Server, "Failed to create the QuickJS context");
        JS_FreeRuntime(mRuntime);
        mRuntime = nullptr;
        return;
    }

    _installConsole();
}

ScriptEngine::~ScriptEngine() {
    mApi.reset();

    if (mContext != nullptr)
        JS_FreeContext(mContext);

    if (mRuntime != nullptr)
        JS_FreeRuntime(mRuntime);
}

void ScriptEngine::bindHost(ServerNetworkHandler &host) {
    if (mContext == nullptr || mApi != nullptr)
        return;

    mExecutionDeadlineMs = nowMs() + mWatchdogMs;

    mApi.reset(new ScriptApi(mContext, mRuntime, host));
    mApi->install();
}

void ScriptEngine::_installConsole() {
    JSValue global = JS_GetGlobalObject(mContext);
    JSValue console = JS_NewObject(mContext);

    JS_SetPropertyStr(mContext, console, "log", JS_NewCFunction(mContext, consoleLog, "log", 1));
    JS_SetPropertyStr(mContext, console, "warn", JS_NewCFunction(mContext, consoleWarn, "warn", 1));
    JS_SetPropertyStr(mContext, console, "error", JS_NewCFunction(mContext, consoleWarn, "error", 1));

    JS_SetPropertyStr(mContext, global, "console", console);
    JS_FreeValue(mContext, global);
}

bool ScriptEngine::_checkException(bool hadError, const std::string &name) {
    if (!hadError)
        return true;

    JSValue exception = JS_GetException(mContext);

    const std::string message = valueToString(mContext, exception);
    LOG_ERROR(LogAreaID::Server, "Script error in %s: %s", name.c_str(), message.c_str());

    JSValue stack = JS_GetPropertyStr(mContext, exception, "stack");
    if (!JS_IsUndefined(stack)) {
        const std::string trace = valueToString(mContext, stack);
        if (!trace.empty())
            LOG_ERROR(LogAreaID::Server, "%s", trace.c_str());
    }
    JS_FreeValue(mContext, stack);

    JS_FreeValue(mContext, exception);
    return false;
}

bool ScriptEngine::evaluate(const std::string &source, const std::string &name, bool asModule) {
    if (mContext == nullptr)
        return false;

    mExecutionDeadlineMs = nowMs() + mWatchdogMs;

    const int flags = asModule ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL;
    JSValue result = JS_Eval(mContext, source.c_str(), source.size(), name.c_str(), flags);
    const bool ok = _checkException(JS_IsException(result), name);
    JS_FreeValue(mContext, result);

    _pumpJobs();
    return ok;
}

bool ScriptEngine::evaluateFile(const std::string &path) {
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR(LogAreaID::Server, "Could not open script %s", path.c_str());
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    return evaluate(buffer.str(), path, true);
}

void ScriptEngine::_pumpJobs() {
    JSContext *pending = nullptr;

    for (;;) {
        const int status = JS_ExecutePendingJob(mRuntime, &pending);
        if (status <= 0) {
            if (status < 0)
                _checkException(true, "microtask");
            break;
        }
    }
}

void ScriptEngine::onProjectileHitBlock(ServerActor &projectile, int32_t x, int32_t y, int32_t z) {
    if (mApi == nullptr)
        return;

    mExecutionDeadlineMs = nowMs() + mWatchdogMs;
    mApi->emitProjectileHitBlock(projectile, x, y, z);
    _pumpJobs();
}

void ScriptEngine::onItemUseOnBlock(ServerPlayer &player, int32_t x, int32_t y, int32_t z) {
    if (mApi == nullptr)
        return;

    mExecutionDeadlineMs = nowMs() + mWatchdogMs;
    mApi->emitItemUseOnBlock(player, x, y, z);
    _pumpJobs();
}

void ScriptEngine::onWorldInitialize() {
    if (mApi == nullptr)
        return;

    mExecutionDeadlineMs = nowMs() + mWatchdogMs;
    mApi->emitWorldInitialize();
    mApi->emitWorldLoad();
    _pumpJobs();
}

void ScriptEngine::tick(int64_t currentTick) {
    if (mContext == nullptr)
        return;

    mExecutionDeadlineMs = nowMs() + mWatchdogMs;

    if (mApi != nullptr)
        mApi->tick(currentTick);

    _pumpJobs();
}
