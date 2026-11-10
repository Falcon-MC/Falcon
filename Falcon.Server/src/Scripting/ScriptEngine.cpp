#include "Scripting/ScriptEngine.h"

#include <algorithm>

#include "World/Actor/MobEffect.h"
#include "World/Actor/ServerActor.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Event/EventBus.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/Binding/ScriptApi.h"
#include "Scripting/Content/CustomContentRegistry.h"

#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace ScriptValues;

namespace {
    int64_t nowMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                .count();
    }

    const char *directionName(int32_t face) {
        static const char *NAMES[] = {"Down", "Up", "North", "South", "West", "East"};
        return face >= 0 && face < 6 ? NAMES[face] : "Up";
    }

    void fillBlockInteraction(ScriptApi &api, JSValue event, ServerPlayer &player, const Vector3i &position,
                              int32_t face) {
        JSContext *ctx = api.context();
        JS_SetPropertyStr(ctx, event, "player", api.makePlayer(player));
        JS_SetPropertyStr(ctx, event, "block",
                          api.makeBlock(api.host().getLevelFor(player), position.x, position.y, position.z));
        JS_SetPropertyStr(ctx, event, "blockFace", JS_NewString(ctx, directionName(face)));
        JS_SetPropertyStr(ctx, event, "itemStack", api.makeItem(player.getInventory().getItemInHand()));
        JS_SetPropertyStr(ctx, event, "isFirstEvent", JS_TRUE);
    }

    JSValue hitGetter(JSContext *ctx, JSValueConst, int, JSValueConst *, int, JSValue *data) {
        return JS_DupValue(ctx, data[0]);
    }

    void logLine(JSContext *ctx, int argc, JSValueConst *argv, bool warn) {
        std::ostringstream line;
        for (int i = 0; i < argc; ++i) {
            if (i != 0)
                line << ' ';
            line << toString(ctx, argv[i]);
        }

        const ScriptApi *api = ScriptApi::fromContext(ctx);
        const char *pack = api == nullptr ? "script" : api->packName().c_str();
        if (warn)
            LOG_WARN(LogAreaID::Script, "[%s] %s", pack, line.str().c_str());
        else
            LOG_INFO(LogAreaID::Script, "[%s] %s", pack, line.str().c_str());
    }

    JSValue consoleLog(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        logLine(ctx, argc, argv, false);
        return JS_UNDEFINED;
    }

    JSValue consoleWarn(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        logLine(ctx, argc, argv, true);
        return JS_UNDEFINED;
    }

    void logException(JSContext *ctx, const std::string &pack, const std::string &where) {
        JSValue exception = JS_GetException(ctx);
        LOG_ERROR(LogAreaID::Script, "[%s] Script error in %s: %s", pack.c_str(), where.c_str(),
                  toString(ctx, exception).c_str());

        JSValue stack = JS_GetPropertyStr(ctx, exception, "stack");
        if (JS_IsString(stack)) {
            const std::string trace = toString(ctx, stack);
            if (!trace.empty())
                LOG_ERROR(LogAreaID::Script, "%s", trace.c_str());
        }
        JS_FreeValue(ctx, stack);
        JS_FreeValue(ctx, exception);
    }

    // collapses "." and ".." and keeps a leading slash
    std::string canonicalPath(const std::string &path) {
        std::vector<std::string> segments;
        size_t start = 0;
        while (start <= path.size()) {
            size_t end = path.find('/', start);
            if (end == std::string::npos)
                end = path.size();

            const std::string segment = path.substr(start, end - start);
            if (segment == "..") {
                if (!segments.empty())
                    segments.pop_back();
            } else if (segment != "." && !segment.empty()) {
                segments.push_back(segment);
            }
            start = end + 1;
        }

        std::string result = !path.empty() && path[0] == '/' ? "/" : "";
        for (size_t i = 0; i < segments.size(); ++i) {
            if (i != 0)
                result += "/";
            result += segments[i];
        }
        return result;
    }

    std::string readFile(const std::string &path) {
        std::ifstream file(path.c_str(), std::ios::binary);
        if (!file.is_open())
            return std::string();

        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    char *scriptModuleNormalize(JSContext *ctx, const char *baseName, const char *name, void *) {
        std::string normalized = name;
        if (!normalized.empty() && normalized[0] == '.') {
            const std::string base(baseName);
            const size_t slash = base.find_last_of('/');
            normalized = canonicalPath((slash == std::string::npos ? std::string() : base.substr(0, slash + 1))
                                       + normalized);
        }

        char *result = (char *) js_malloc(ctx, normalized.size() + 1);
        if (result == nullptr)
            return nullptr;
        std::memcpy(result, normalized.c_str(), normalized.size() + 1);
        return result;
    }

    // native modules a pack declares are already registered: anything else must be a file inside its pack
    JSModuleDef *scriptModuleLoader(JSContext *ctx, const char *moduleName, void *) {
        const ScriptApi *api = ScriptApi::fromContext(ctx);
        const std::string name(moduleName);

        if (name.compare(0, 11, "@minecraft/") == 0) {
            const ScriptModuleVersion *module = api->modules().find(name);
            if (module != nullptr && !module->mImplemented)
                JS_ThrowReferenceError(ctx, "could not load module '%s': not available on this server", moduleName);
            else
                JS_ThrowReferenceError(ctx, "could not load module '%s'", moduleName);
            return nullptr;
        }

        // canonical first: a prefix test on "<root>/../.." would pass. windows walks up through either
        // separator, so both count
        std::string slashed = name;
        std::replace(slashed.begin(), slashed.end(), '\\', '/');
        if (canonicalPath(slashed) != slashed) {
            JS_ThrowReferenceError(ctx, "could not load module '%s'", moduleName);
            return nullptr;
        }

        std::string path = name;
        if (path.size() < 3 || path.compare(path.size() - 3, 3, ".js") != 0)
            path += ".js";
        const std::string root = canonicalPath(api->packRoot()) + "/";
        std::string source;
        if (path.compare(0, root.size(), root) == 0)
            source = readFile(path);
        if (source.empty() && path != name && name.compare(0, root.size(), root) == 0)
            source = readFile(name);
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

    void finalizeNative(JSRuntime *, JSValueConst value) {
        delete (ScriptNative *) JS_GetOpaque(value, JS_GetClassID(value));
    }
}

int _scriptInterruptHandler(JSRuntime *, void *opaque) {
    const ScriptEngine *engine = (const ScriptEngine *) opaque;
    if (engine == nullptr)
        return 0;

    return nowMs() > engine->mExecutionDeadlineMs ? 1 : 0;
}

ScriptEngine::ScriptEngine() : mRuntime(nullptr), mWatchdogMs(DEFAULT_WATCHDOG_MS), mExecutionDeadlineMs(0) {
    mRuntime = JS_NewRuntime();
    if (mRuntime == nullptr) {
        LOG_ERROR(LogAreaID::Script, "Failed to create the QuickJS runtime");
        return;
    }

    // behavior pack scripts are untrusted: cap heap and native stack; the watchdog only bounds run time
    JS_SetMemoryLimit(mRuntime, SCRIPT_MEMORY_LIMIT_BYTES);
    JS_SetMaxStackSize(mRuntime, SCRIPT_MAX_STACK_BYTES);

    JS_SetInterruptHandler(mRuntime, _scriptInterruptHandler, this);
    JS_SetModuleLoaderFunc(mRuntime, scriptModuleNormalize, scriptModuleLoader, nullptr);

    JS_NewClassID(mRuntime, &mNativeClassId);
    JSClassDef nativeClass{};
    nativeClass.class_name = "Object";
    nativeClass.finalizer = finalizeNative;
    JS_NewClass(mRuntime, mNativeClassId, &nativeClass);
}

ScriptEngine::~ScriptEngine() {
    MobEffectManager::setAddFilter(nullptr);

    for (std::unique_ptr<ScriptApi> &api: mApis) {
        JSContext *context = api->context();
        api.reset();
        JS_FreeContext(context);
    }
    mApis.clear();

    if (mRuntime != nullptr)
        JS_FreeRuntime(mRuntime);
}

void ScriptEngine::bindHost(ServerNetworkHandler &host) {
    if (mRuntime == nullptr || mHost != nullptr)
        return;

    mHost = &host;
    MobEffectManager::setAddFilter(
            [this](Actor &actor, const MobEffectInstance &effect) { return !beforeEffectAdd(actor, effect); });

    // once for the server, not per pack: every pack sees the same custom block ticking
    host.getEventBus().after().mPlayerPlaceBlock.subscribe([this](PlayerPlaceBlockAfterEvent &event) {
        _scheduleBlockTick(mHost->getLevelFor(event.mPlayer), event.mBlockIdentifier, event.mBlockPosition);
    });
}

void ScriptEngine::_arm() {
    mExecutionDeadlineMs = nowMs() + mWatchdogMs;
}

void ScriptEngine::_installConsole(JSContext *context) {
    JSValue global = JS_GetGlobalObject(context);
    JSValue console = JS_NewObject(context);

    JS_SetPropertyStr(context, console, "log", JS_NewCFunction(context, consoleLog, "log", 1));
    JS_SetPropertyStr(context, console, "info", JS_NewCFunction(context, consoleLog, "info", 1));
    JS_SetPropertyStr(context, console, "warn", JS_NewCFunction(context, consoleWarn, "warn", 1));
    JS_SetPropertyStr(context, console, "error", JS_NewCFunction(context, consoleWarn, "error", 1));

    JS_SetPropertyStr(context, global, "console", console);
    JS_FreeValue(context, global);
}

size_t ScriptEngine::loadPacks(const std::vector<BehaviorPack> &packs) {
    size_t loaded = 0;
    for (const BehaviorPack &pack: packs) {
        if (pack.hasScript() && _loadPack(pack))
            loaded++;
    }
    return loaded;
}

bool ScriptEngine::_loadPack(const BehaviorPack &pack) {
    if (mHost == nullptr)
        return false;

    ScriptModuleSet modules;
    std::string error;
    if (!resolveScriptModules(pack.mName, pack.mScriptModuleUuid, pack.mScriptModules, modules, error)) {
        LOG_ERROR(LogAreaID::Script, "%s", error.c_str());
        return false;
    }

    for (const ScriptModuleVersion &module: modules.getModules()) {
        if (!module.mImplemented)
            LOG_WARN(LogAreaID::Script, "[%s] module [%s] is not available on this server: importing it fails",
                     pack.mName.c_str(), module.mName.c_str());
    }

    const std::string source = readFile(pack.scriptPath());
    if (source.empty()) {
        LOG_ERROR(LogAreaID::Script, "Could not open script %s", pack.scriptPath().c_str());
        return false;
    }

    JSContext *context = JS_NewContext(mRuntime);
    if (context == nullptr) {
        LOG_ERROR(LogAreaID::Script, "Plugin [%s] - failed to create context.", pack.mName.c_str());
        return false;
    }
    _installConsole(context);

    std::unique_ptr<ScriptApi> api(
            new ScriptApi(context, *mHost, mPlayers, mNativeClassId, pack.mName, canonicalPath(pack.mPath), modules));
    _arm();
    api->install();

    // 2.0 starts every script in early execution; 1.x scripts run with the world already up
    if (!api->isStable1())
        api->setPrivilege(ScriptPrivilege::Early);

    const std::string entry = canonicalPath(pack.scriptPath());
    _arm();
    JSValue result = JS_Eval(context, source.c_str(), source.size(), entry.c_str(), JS_EVAL_TYPE_MODULE);
    const bool ok = !JS_IsException(result);
    if (!ok)
        logException(context, pack.mName, entry);
    JS_FreeValue(context, result);

    mApis.push_back(std::move(api));
    _pumpJobs();
    return ok;
}

void ScriptEngine::startup() {
    for (std::unique_ptr<ScriptApi> &api: mApis) {
        _arm();
        api->startup();
        _pumpJobs();
    }
}

void ScriptEngine::shutdown() {
    for (std::unique_ptr<ScriptApi> &api: mApis) {
        _arm();
        api->shutdown();
        _pumpJobs();
    }
}

void ScriptEngine::_pumpJobs() {
    JSContext *pending = nullptr;

    for (;;) {
        const int status = JS_ExecutePendingJob(mRuntime, &pending);
        if (status <= 0) {
            if (status < 0 && pending != nullptr) {
                const ScriptApi *api = ScriptApi::fromContext(pending);
                logException(pending, api == nullptr ? std::string("script") : api->packName(), "microtask");
            }
            break;
        }
    }
}

template <typename Fill>
bool ScriptEngine::_emit(const char *key, const char *signal, bool cancellable, Fill fill) {
    bool cancelled = false;
    for (std::unique_ptr<ScriptApi> &api: mApis) {
        if (!api->hasSubscribers(key))
            continue;

        _arm();
        JSValue event = api->makeEvent(signal);
        fill(*api, event);
        if (cancellable)
            JS_SetPropertyStr(api->context(), event, "cancel", JS_FALSE);
        cancelled = api->emit(key, event) || cancelled;
        _pumpJobs();
    }
    return cancellable && cancelled;
}

void ScriptEngine::onProjectileHitBlock(ServerActor &projectile, int32_t x, int32_t y, int32_t z) {
    for (std::unique_ptr<ScriptApi> &api: mApis) {
        _arm();
        api->emitProjectileHitBlock(projectile, x, y, z);
        _pumpJobs();
    }
}

void ScriptEngine::onItemUseOnBlock(ServerPlayer &player, int32_t x, int32_t y, int32_t z) {
    for (std::unique_ptr<ScriptApi> &api: mApis) {
        _arm();
        api->emitItemUseOnBlock(player, x, y, z);
        _pumpJobs();
    }
}

void ScriptEngine::onProjectileHitEntity(ServerActor &projectile, Actor &hitEntity, const Vector3f &location) {
    _emit("after.projectileHitEntity", "WorldAfterEvents.projectileHitEntity", false,
          [&](ScriptApi &api, JSValue event) {
              JSContext *ctx = api.context();
              JS_SetPropertyStr(ctx, event, "projectile", api.makeActor(projectile));
              JS_SetPropertyStr(ctx, event, "location", vector3(ctx, location.x, location.y, location.z));
              JS_SetPropertyStr(ctx, event, "dimension", api.makeDimension(api.host().getLevelFor(projectile)));

              if (projectile.hasOwnerPlayer()) {
                  ServerPlayer *owner = api.resolvePlayerByHandle(projectile.getOwnerPlayerHandle());
                  if (owner != nullptr)
                      JS_SetPropertyStr(ctx, event, "source", api.makePlayer(*owner));
              } else if (projectile.getOwnerUniqueId() >= 0) {
                  ServerActor *owner = api.host().getActor(projectile.getOwnerUniqueId());
                  if (owner != nullptr)
                      JS_SetPropertyStr(ctx, event, "source", api.makeActor(*owner));
              }

              JSValue hit = JS_NewObject(ctx);
              JS_SetPropertyStr(ctx, hit, "entity", api.makeEntity(hitEntity));
              JSValue getter = JS_NewCFunctionData(ctx, hitGetter, 0, 0, 1, &hit);
              JS_FreeValue(ctx, hit);
              JS_SetPropertyStr(ctx, event, "getEntityHit", getter);
          });
}

void ScriptEngine::onEntityHitBlock(Actor &damagingEntity, const Vector3i &position, int32_t face) {
    _emit("after.entityHitBlock", "WorldAfterEvents.entityHitBlock", false, [&](ScriptApi &api, JSValue event) {
        JSContext *ctx = api.context();
        JS_SetPropertyStr(ctx, event, "damagingEntity", api.makeEntity(damagingEntity));
        JS_SetPropertyStr(ctx, event, "hitBlock",
                          api.makeBlock(api.host().getLevelFor(damagingEntity), position.x, position.y, position.z));
        JS_SetPropertyStr(ctx, event, "blockFace", JS_NewString(ctx, directionName(face)));
    });
}

void ScriptEngine::onEntityHitEntity(Actor &damagingEntity, Actor &hitEntity) {
    _emit("after.entityHitEntity", "WorldAfterEvents.entityHitEntity", false, [&](ScriptApi &api, JSValue event) {
        JS_SetPropertyStr(api.context(), event, "damagingEntity", api.makeEntity(damagingEntity));
        JS_SetPropertyStr(api.context(), event, "hitEntity", api.makeEntity(hitEntity));
    });
}

void ScriptEngine::onEntityItemDrop(Actor &entity, ItemActor *dropped) {
    _emit("after.entityItemDrop", "WorldAfterEvents.entityItemDrop", false, [&](ScriptApi &api, JSValue event) {
        JSContext *ctx = api.context();
        JS_SetPropertyStr(ctx, event, "entity", api.makeEntity(entity));
        JSValue items = JS_NewArray(ctx);
        if (dropped != nullptr)
            JS_SetPropertyUint32(ctx, items, 0, api.makeEntity(*dropped));
        JS_SetPropertyStr(ctx, event, "items", items);
    });
}

void ScriptEngine::onPlayerInteractWithBlock(ServerPlayer &player, const std::string &blockId, const Vector3i &position,
                                             int32_t face) {
    _emit("after.playerInteractWithBlock", "WorldAfterEvents.playerInteractWithBlock", false,
          [&](ScriptApi &api, JSValue event) { fillBlockInteraction(api, event, player, position, face); });

    for (std::unique_ptr<ScriptApi> &api: mApis) {
        _arm();
        api->emitBlockInteract(player, blockId, position, directionName(face));
        _pumpJobs();
    }
}

void ScriptEngine::_scheduleBlockTick(Level &level, const std::string &blockId, const Vector3i &position) {
    for (const CustomBlockDefinition &definition: CustomContentRegistry::getInstance().getBlocks()) {
        if (definition.mIdentifier != blockId)
            continue;
        if (definition.mHasTick) {
            std::uniform_int_distribution<int32_t> delay(definition.mTickMin, definition.mTickMax);
            level.scheduleBlockUpdate(position, delay(mTickRandom));
        }
        return;
    }
}

void ScriptEngine::onCustomBlockTick(Level &level, const std::string &blockId, const Vector3i &position) {
    for (std::unique_ptr<ScriptApi> &api: mApis) {
        _arm();
        api->emitBlockTick(level, blockId, position);
        _pumpJobs();
    }

    for (const CustomBlockDefinition &definition: CustomContentRegistry::getInstance().getBlocks()) {
        if (definition.mIdentifier != blockId)
            continue;
        if (definition.mTickLooping)
            _scheduleBlockTick(level, blockId, position);
        return;
    }
}

void ScriptEngine::onCustomBlockStep(Actor &actor, const Level &level, const std::string &blockId,
                                     const Vector3i &position) {
    for (std::unique_ptr<ScriptApi> &api: mApis) {
        _arm();
        api->emitBlockStep(actor, level, blockId, position);
        _pumpJobs();
    }
}

void ScriptEngine::onCustomBlockStepOff(Actor &actor, const Level &level) {
    for (std::unique_ptr<ScriptApi> &api: mApis) {
        if (!api->hasSteppingActors())
            continue;
        _arm();
        api->emitBlockStepOff(actor, level);
        _pumpJobs();
    }
}

void ScriptEngine::onEntitySpawn(ServerActor &entity) {
    _emit("after.entitySpawn", "WorldAfterEvents.entitySpawn", false, [&](ScriptApi &api, JSValue event) {
        JS_SetPropertyStr(api.context(), event, "entity", api.makeActor(entity));
        JS_SetPropertyStr(api.context(), event, "cause", JS_NewString(api.context(), "Spawned"));
    });
}

void ScriptEngine::onEntityRemove(ServerActor &entity) {
    _emit("after.entityRemove", "WorldAfterEvents.entityRemove", false, [&](ScriptApi &api, JSValue event) {
        JSContext *ctx = api.context();
        JS_SetPropertyStr(ctx, event, "removedEntityId", string(ctx, std::to_string(entity.getUniqueId())));
        JS_SetPropertyStr(ctx, event, "typeId", string(ctx, entity.getTypeId()));
    });

    for (std::unique_ptr<ScriptApi> &api: mApis)
        api->forgetEntity(entity.getUniqueId());
}

void ScriptEngine::onEntityLoad(ServerActor &entity) {
    _emit("after.entityLoad", "WorldAfterEvents.entityLoad", false, [&](ScriptApi &api, JSValue event) {
        JS_SetPropertyStr(api.context(), event, "entity", api.makeActor(entity));
    });
}

bool ScriptEngine::beforePlayerInteractWithBlock(ServerPlayer &player, const Vector3i &position, int32_t face) {
    return _emit("before.playerInteractWithBlock", "WorldBeforeEvents.playerInteractWithBlock", true,
                 [&](ScriptApi &api, JSValue event) { fillBlockInteraction(api, event, player, position, face); });
}

bool ScriptEngine::beforeEntityHurt(Actor &hurtEntity, float damage, const std::string &cause, const Actor *attacker) {
    return _emit("before.entityHurt", "WorldBeforeEvents.entityHurt", true, [&](ScriptApi &api, JSValue event) {
        JSContext *ctx = api.context();
        JS_SetPropertyStr(ctx, event, "hurtEntity", api.makeEntity(hurtEntity));
        JS_SetPropertyStr(ctx, event, "damage", JS_NewFloat64(ctx, damage));

        JSValue source = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, source, "cause", string(ctx, cause));
        if (attacker != nullptr)
            JS_SetPropertyStr(ctx, source, "damagingEntity", api.makeEntity(const_cast<Actor &>(*attacker)));
        JS_SetPropertyStr(ctx, event, "damageSource", source);
    });
}

bool ScriptEngine::beforeEffectAdd(Actor &entity, const MobEffectInstance &effect) {
    return _emit("before.effectAdd", "WorldBeforeEvents.effectAdd", true, [&](ScriptApi &api, JSValue event) {
        JSContext *ctx = api.context();
        JS_SetPropertyStr(ctx, event, "entity", api.makeEntity(entity));
        JS_SetPropertyStr(ctx, event, "effectType", JS_NewString(ctx, getMobEffectName(effect.mId)));
        JS_SetPropertyStr(ctx, event, "duration", JS_NewInt32(ctx, effect.mDuration));
    });
}

bool ScriptEngine::beforePlayerInteractWithEntity(ServerPlayer &player, Actor &target) {
    return _emit("before.playerInteractWithEntity", "WorldBeforeEvents.playerInteractWithEntity", true,
                 [&](ScriptApi &api, JSValue event) {
                     JSContext *ctx = api.context();
                     JS_SetPropertyStr(ctx, event, "player", api.makePlayer(player));
                     JS_SetPropertyStr(ctx, event, "target", api.makeEntity(target));
                     JS_SetPropertyStr(ctx, event, "itemStack", api.makeItem(player.getInventory().getItemInHand()));
                 });
}

bool ScriptEngine::beforeItemUse(ServerPlayer &player) {
    const ItemStack &held = player.getInventory().getItemInHand();
    if (held.isAir() || held.mDefinition == nullptr)
        return false;

    return _emit("before.itemUse", "WorldBeforeEvents.itemUse", true, [&](ScriptApi &api, JSValue event) {
        JS_SetPropertyStr(api.context(), event, "source", api.makePlayer(player));
        JS_SetPropertyStr(api.context(), event, "itemStack", api.makeItem(held));
    });
}

void ScriptEngine::tick(int64_t currentTick) {
    for (std::unique_ptr<ScriptApi> &api: mApis) {
        _arm();
        api->tick(currentTick);
    }
    _pumpJobs();
}
