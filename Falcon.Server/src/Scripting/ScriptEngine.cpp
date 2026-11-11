#include "Scripting/ScriptEngine.h"

#include <algorithm>

#include "World/Actor/MobEffect.h"
#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/ServerActor.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Event/EventBus.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/Binding/ScriptApi.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "Protocol/Packets/SyncWorldClocksPacket.h"

#include <chrono>
#include <cmath>
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

    const CustomItemDefinition *heldCustomItem(ServerPlayer &player) {
        const ItemStack &held = player.getInventory().getItemInHand();
        if (held.isAir() || held.mDefinition == nullptr)
            return nullptr;
        return CustomContentRegistry::getInstance().findItem(held.mDefinition->getIdentifier());
    }

    // the states the block has where it stands, its defaults once it is gone or unloaded
    Tag statesAt(const CustomBlockDefinition &block, const Level &level, const Vector3i &position) {
        const BlockState *state = const_cast<Level &>(level).peekBlockPtr(position.x, position.y, position.z);
        if (state != nullptr && state->mName == block.mIdentifier)
            return state->mStates;
        return CustomContentRegistry::defaultStatesOf(block);
    }

    std::vector<CustomComponentUse> usesAt(const CustomBlockDefinition &block, const Level &level,
                                           const Vector3i &position) {
        return CustomContentRegistry::componentsFor(block, statesAt(block, level, position));
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
    MobEffectManager::setAddListener(nullptr);
    Actor::setHealHooks(nullptr, nullptr);
    Actor::setHealthListener(nullptr);

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
            [this](Actor &actor, MobEffectInstance &effect) { return !beforeEffectAdd(actor, effect); });
    MobEffectManager::setAddListener(
            [this](Actor &actor, const MobEffectInstance &effect) { onEffectAdd(actor, effect); });
    Actor::setHealHooks(
            [this](Actor &actor, float &amount, HealCause cause) {
                return !_beforeHeal(actor, amount, (int32_t) cause);
            },
            [this](Actor &actor, float amount, HealCause cause) { _onHeal(actor, amount, (int32_t) cause); });
    Actor::setHealthListener(
            [this](Actor &actor, float oldValue, float newValue) { _onHealthChanged(actor, oldValue, newValue); });
    _bindClockListener();
}

void ScriptEngine::_bindClockListener() {
    WorldClockSystem::Listener listener;
    const auto fire = [this](const char *key, const char *signal, const WorldClock &clock,
                             const WorldClockMarker *marker, int32_t *newTime) {
        bool cancelled = false;
        for (std::unique_ptr<ScriptApi> &api: mApis) {
            if (!api->hasSubscribers(key))
                continue;
            _arm();
            JSContext *ctx = api->context();
            JSValue event = api->makeEvent(signal);
            JS_SetPropertyStr(ctx, event, "clock", api->makeClock(clock.mName));
            if (marker != nullptr)
                JS_SetPropertyStr(ctx, event, "timeMarker", api->makeTimeMarker(clock.mName, marker->mName));
            if (newTime != nullptr) {
                JS_SetPropertyStr(ctx, event, "cancel", JS_FALSE);
                JS_SetPropertyStr(ctx, event, "newTime", JS_NewInt32(ctx, *newTime));
                // emit frees the event: keep it to read newTime back
                JS_DupValue(ctx, event);
            }
            cancelled = api->emitClockEvent(key, event, clock.mName, marker == nullptr ? std::string() : marker->mName)
                        || cancelled;
            if (newTime != nullptr) {
                JSValue value = JS_GetPropertyStr(ctx, event, "newTime");
                double ticks = 0.0;
                if (JS_ToFloat64(ctx, &ticks, value) == 0 && std::isfinite(ticks) && ticks >= 0.0 && ticks <= INT32_MAX)
                    *newTime = (int32_t) ticks;
                JS_FreeValue(ctx, value);
                JS_FreeValue(ctx, event);
            }
            _pumpJobs();
        }
        return cancelled;
    };
    listener.mTimeMarker = [fire](const WorldClock &clock, const WorldClockMarker &marker) {
        fire("after.worldClockOnTimeMarker", "WorldAfterEvents.worldClockOnTimeMarker", clock, &marker, nullptr);
    };
    listener.mTimeModified = [fire](const WorldClock &clock) {
        fire("after.worldClockOnTimeModified", "WorldAfterEvents.worldClockOnTimeModified", clock, nullptr, nullptr);
    };
    listener.mPauseChanged = [fire](const WorldClock &clock, bool paused) {
        if (paused)
            fire("after.worldClockOnPaused", "WorldAfterEvents.worldClockOnPaused", clock, nullptr, nullptr);
        else
            fire("after.worldClockOnResumed", "WorldAfterEvents.worldClockOnResumed", clock, nullptr, nullptr);
    };
    listener.mRestart = [fire](const WorldClock &clock, int32_t &newTime) {
        return fire("before.worldClockOnRestart", "WorldBeforeEvents.worldClockOnRestart", clock, nullptr, &newTime);
    };
    listener.mStateChanged = [this](const WorldClock &clock) {
        SyncWorldClocksPacket packet;
        WorldClockSystem::writeState(clock, packet);
        _broadcastClocks(packet);
    };
    listener.mMarkerChanged = [this](const WorldClock &clock, const WorldClockMarker &marker, bool added) {
        SyncWorldClocksPacket packet;
        if (added) {
            packet.mType = SyncWorldClocksType::AddTimeMarker;
            packet.mAddClockId = WorldClockSystem::idOf(clock.mName);
            TimeMarkerData data;
            data.mId = WorldClockSystem::idOf(marker.mName);
            data.mName = marker.mName;
            data.mTime = marker.mTime;
            data.mHasPeriod = marker.mHasPeriod;
            data.mPeriod = marker.mPeriod;
            packet.mAddTimeMarkers.push_back(data);
        } else {
            packet.mType = SyncWorldClocksType::RemoveTimeMarker;
            packet.mRemoveClockId = WorldClockSystem::idOf(clock.mName);
            packet.mRemoveTimeMarkerIds.push_back(WorldClockSystem::idOf(marker.mName));
        }
        _broadcastClocks(packet);
    };
    mClocks.setListener(std::move(listener));
}

void ScriptEngine::_broadcastClocks(const SyncWorldClocksPacket &packet) {
    if (mHost == nullptr || !mHost->isTransportReady())
        return;
    for (auto &entry: mHost->getPlayers()) {
        if (entry.second.isSpawned())
            mHost->getNetworkHandler().send(entry.first, packet, mHost->getCodecContext());
    }
}

void ScriptEngine::saveWorldClocks() {
    if (mHost != nullptr)
        mHost->getWorlds().getDefault().getOverworld().saveSavedData(WorldClockSystem::STORAGE_KEY, mClocks.save(),
                                                                     false);
}

void ScriptEngine::sendWorldClocks(ServerPlayer &player) {
    if (mHost == nullptr || !mHost->isTransportReady())
        return;
    SyncWorldClocksPacket packet;
    mClocks.writeRegistry(packet);
    mHost->getNetworkHandler().send(player.getNetworkIdentifier(), packet, mHost->getCodecContext());
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
    api->setPackSettings(pack.mSettings);
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
    // before startup: packs register their clocks during it
    Tag saved;
    if (mHost != nullptr
        && mHost->getWorlds().getDefault().getOverworld().loadSavedData(WorldClockSystem::STORAGE_KEY, saved, false))
        mClocks.load(saved);
    if (mHost != nullptr) {
        Level &overworld = mHost->getWorlds().getDefault().getOverworld();
        WorldClockSystem::LevelTime time;
        time.mGet = [&overworld]() { return overworld.getTime(); };
        time.mSet = [&overworld](int64_t value) { overworld.setTime(value); };
        time.mSetPaused = [&overworld](bool paused) { overworld.setTimePaused(paused); };
        mClocks.bindOverworld(std::move(time));
    }

    for (std::unique_ptr<ScriptApi> &api: mApis) {
        _arm();
        api->startup();
        _pumpJobs();
    }
    createCustomDimensions();
    for (std::unique_ptr<ScriptApi> &api: mApis) {
        _arm();
        api->worldLoad();
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

void ScriptEngine::onItemUseOnBlock(ServerPlayer &player, const Vector3i &position, int32_t face,
                                    const Vector3f &clickPosition) {
    for (std::unique_ptr<ScriptApi> &api: mApis) {
        _arm();
        api->emitItemUseOnBlock(player, position.x, position.y, position.z);
        _pumpJobs();
    }

    const CustomItemDefinition *item = heldCustomItem(player);
    if (item == nullptr)
        return;

    Level &level = mHost->getLevelFor(player);
    const std::string usedOn = level.getBlockState(position.x, position.y, position.z).mName;
    _fireComponent(false, item->mCustomComponents, "onUseOn", "ItemComponentUseOnEvent",
                   [&](ScriptApi &api, JSValue event) {
                       JSContext *ctx = api.context();
                       JS_SetPropertyStr(ctx, event, "source", api.makePlayer(player));
                       JS_SetPropertyStr(ctx, event, "block", api.makeBlock(level, position.x, position.y, position.z));
                       JS_SetPropertyStr(ctx, event, "blockFace", JS_NewString(ctx, directionName(face)));
                       JS_SetPropertyStr(ctx, event, "faceLocation",
                                         vector3(ctx, clickPosition.x, clickPosition.y, clickPosition.z));
                       JS_SetPropertyStr(ctx, event, "itemStack", api.makeItem(player.getInventory().getItemInHand()));
                       JS_SetPropertyStr(ctx, event, "usedOnBlockPermutation", api.makePermutation(usedOn));
                   });
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
                                             int32_t face, const Vector3f &clickPosition) {
    _emit("after.playerInteractWithBlock", "WorldAfterEvents.playerInteractWithBlock", false,
          [&](ScriptApi &api, JSValue event) { fillBlockInteraction(api, event, player, position, face); });

    const CustomBlockDefinition *block = CustomContentRegistry::getInstance().findBlock(blockId);
    if (block == nullptr)
        return;

    Level &level = mHost->getLevelFor(player);
    _fireComponent(true, usesAt(*block, level, position), "onPlayerInteract", "BlockComponentPlayerInteractEvent",
                   [&](ScriptApi &api, JSValue event) {
                       JSContext *ctx = api.context();
                       api.fillBlockEvent(event, level, position);
                       JS_SetPropertyStr(ctx, event, "player", api.makePlayer(player));
                       JS_SetPropertyStr(ctx, event, "face", JS_NewString(ctx, directionName(face)));
                       JS_SetPropertyStr(ctx, event, "faceLocation",
                                         vector3(ctx, clickPosition.x, clickPosition.y, clickPosition.z));
                   });
}

template <typename Fill, typename Read>
void ScriptEngine::_fireComponent(bool blocks, const std::vector<CustomComponentUse> &uses, const char *hook,
                                  const char *eventClass, Fill fill, Read read) {
    if (uses.empty())
        return;

    for (std::unique_ptr<ScriptApi> &api: mApis) {
        if (!api->handlesHook(blocks, uses, hook))
            continue;

        _arm();
        JSValue event = api->makeComponentEvent(eventClass);
        if (JS_IsUndefined(event))
            continue;
        fill(*api, event);
        api->fireComponents(blocks, uses, hook, event);
        read(*api, event);
        JS_FreeValue(api->context(), event);
        _pumpJobs();
    }
}

template <typename Fill>
void ScriptEngine::_fireComponent(bool blocks, const std::vector<CustomComponentUse> &uses, const char *hook,
                                  const char *eventClass, Fill fill) {
    _fireComponent(blocks, uses, hook, eventClass, fill, [](ScriptApi &, JSValue) {});
}

bool ScriptEngine::isDimensionPending(const std::string &name) const {
    for (const std::unique_ptr<ScriptApi> &api: mApis) {
        const std::vector<std::string> &pending = api->pendingDimensions();
        if (std::find(pending.begin(), pending.end(), name) != pending.end())
            return true;
    }
    return false;
}

void ScriptEngine::createCustomDimensions() {
    if (mHost == nullptr)
        return;

    // sub_14382A4C0: DimensionNameIdTable keeps each custom name's id across restarts
    Level &overworld = mHost->getWorlds().getDefault().getOverworld();
    Tag table;
    if (!overworld.loadSavedData("DimensionNameIdTable", table, false))
        table = Tag::ofCompound();

    bool added = false;
    for (std::unique_ptr<ScriptApi> &api: mApis) {
        for (const std::string &name: api->pendingDimensions()) {
            const Tag *saved = table.get(name);
            const DimensionType dimension = Dimension::registerCustom(name, saved == nullptr ? -1 : saved->asInt());
            if (saved == nullptr || saved->asInt() != Dimension::toId(dimension)) {
                table.putInt(name, Dimension::toId(dimension));
                added = true;
            }
            for (World *world: mHost->getWorlds().getWorlds())
                world->addCustomLevel(dimension);
        }
    }
    if (added)
        overworld.saveSavedData("DimensionNameIdTable", table, false);
}

bool ScriptEngine::isCustomComponentRegistered(bool blocks, const std::string &name) const {
    for (const std::unique_ptr<ScriptApi> &api: mApis) {
        if (api->hasCustomComponent(blocks, name))
            return true;
    }
    return false;
}

const std::vector<std::string> *ScriptEngine::findCommandEnum(const std::string &name) const {
    for (const std::unique_ptr<ScriptApi> &api: mApis) {
        const std::vector<std::string> *values = api->findCommandEnum(name);
        if (values != nullptr)
            return values;
    }
    return nullptr;
}

void ScriptEngine::runGuarded(const std::function<void()> &call) {
    if (mGuardDepth++ == 0)
        _arm();
    call();
    if (--mGuardDepth == 0)
        _pumpJobs();
}

void ScriptEngine::_scheduleBlockTick(Level &level, const std::string &blockId, const Vector3i &position) {
    const CustomBlockDefinition *definition = CustomContentRegistry::getInstance().findBlock(blockId);
    if (definition == nullptr || !definition->mHasTick)
        return;

    std::uniform_int_distribution<int32_t> delay(definition->mTickMin, definition->mTickMax);
    level.scheduleBlockUpdate(position, delay(mTickRandom));
}

void ScriptEngine::onCustomBlockTick(Level &level, const std::string &blockId, const Vector3i &position) {
    const CustomBlockDefinition *block = CustomContentRegistry::getInstance().findBlock(blockId);
    if (block == nullptr)
        return;

    // the game warns and skips onTick for a block without minecraft:tick
    if (block->mHasTick)
        _fireComponent(true, usesAt(*block, level, position), "onTick", "BlockComponentTickEvent",
                       [&](ScriptApi &api, JSValue event) { api.fillBlockEvent(event, level, position); });

    if (block->mTickLooping)
        _scheduleBlockTick(level, blockId, position);
}

void ScriptEngine::onCustomBlockRandomTick(Level &level, const std::string &blockId, const Vector3i &position) {
    const CustomBlockDefinition *block = CustomContentRegistry::getInstance().findBlock(blockId);
    if (block == nullptr)
        return;

    _fireComponent(true, usesAt(*block, level, position), "onRandomTick", "BlockComponentRandomTickEvent",
                   [&](ScriptApi &api, JSValue event) { api.fillBlockEvent(event, level, position); });
}

void ScriptEngine::onCustomBlockStep(Actor &actor, const Level &level, const std::string &blockId,
                                     const Vector3i &position) {
    const CustomBlockDefinition *block = CustomContentRegistry::getInstance().findBlock(blockId);
    for (std::unique_ptr<ScriptApi> &api: mApis) {
        _arm();
        api->emitBlockStep(actor, level, block, position);
        _pumpJobs();
    }
}

void ScriptEngine::onCustomBlockStepOff(Actor &actor, const Level &level) {
    for (std::unique_ptr<ScriptApi> &api: mApis) {
        if (!api->hasSteppingActors())
            continue;
        _arm();
        api->emitBlockStep(actor, level, nullptr, Vector3i());
        _pumpJobs();
    }
}

void ScriptEngine::onCustomBlockFallOn(Actor &actor, Level &level, const std::string &blockId, const Vector3i &position,
                                       float fallDistance) {
    const CustomBlockDefinition *block = CustomContentRegistry::getInstance().findBlock(blockId);
    if (block == nullptr || !block->mEntityFallOn || fallDistance < block->mMinFallDistance)
        return;

    _fireComponent(true, usesAt(*block, level, position), "onEntityFallOn", "BlockComponentEntityFallOnEvent",
                   [&](ScriptApi &api, JSValue event) {
                       api.fillBlockEvent(event, level, position);
                       JS_SetPropertyStr(api.context(), event, "entity", api.makeEntity(actor));
                       JS_SetPropertyStr(api.context(), event, "fallDistance",
                                         JS_NewFloat64(api.context(), fallDistance));
                   });
}

void ScriptEngine::onCustomBlockRedstone(Level &level, const std::string &blockId, const Vector3i &position,
                                         int32_t power) {
    const CustomBlockDefinition *block = CustomContentRegistry::getInstance().findBlock(blockId);
    if (block == nullptr || !block->mRedstoneConsumer)
        return;

    std::unordered_map<int64_t, int32_t> &powers = mRedstonePower[&level];
    const int64_t key = RedstoneSystem::packPosition(position);
    const auto known = powers.find(key);
    const bool first = known == powers.end();
    const int32_t previous = first ? 0 : known->second;
    if (!first && previous == power)
        return;
    powers[key] = power;

    if (power < block->mRedstoneMinPower || (first && power == 0))
        return;

    _fireComponent(true, usesAt(*block, level, position), "onRedstoneUpdate", "BlockComponentRedstoneUpdateEvent",
                   [&](ScriptApi &api, JSValue event) {
                       JSContext *ctx = api.context();
                       api.fillBlockEvent(event, level, position);
                       JS_SetPropertyStr(ctx, event, "powerLevel", JS_NewInt32(ctx, power));
                       JS_SetPropertyStr(ctx, event, "previousPowerLevel", JS_NewInt32(ctx, previous));
                       if (api.surface().has("BlockComponentRedstoneUpdateEvent.firstUpdate"))
                           JS_SetPropertyStr(ctx, event, "firstUpdate", JS_NewBool(ctx, first));
                   });
}

bool ScriptEngine::beforeCustomBlockPlace(ServerPlayer &player, Level &level, const Vector3i &position, int32_t face,
                                          BlockState &placed) {
    const CustomBlockDefinition *block = CustomContentRegistry::getInstance().findBlock(placed.mName);
    if (block == nullptr)
        return false;

    bool cancelled = false;
    BlockState chosen = placed;
    _fireComponent(
            true, CustomContentRegistry::componentsFor(*block, placed.mStates), "beforeOnPlayerPlace",
            "BlockComponentPlayerPlaceBeforeEvent",
            [&](ScriptApi &api, JSValue event) {
                JSContext *ctx = api.context();
                api.fillBlockEvent(event, level, position);
                JS_SetPropertyStr(ctx, event, "player", api.makePlayer(player));
                JS_SetPropertyStr(ctx, event, "face", JS_NewString(ctx, directionName(face)));
                JS_SetPropertyStr(ctx, event, "permutationToPlace", api.makePermutation(placed));
                JS_SetPropertyStr(ctx, event, "cancel", JS_FALSE);
            },
            [&](ScriptApi &api, JSValue event) {
                JSContext *ctx = api.context();
                JSValue cancel = JS_GetPropertyStr(ctx, event, "cancel");
                cancelled = JS_ToBool(ctx, cancel) == 1 || cancelled;
                JS_FreeValue(ctx, cancel);

                JSValue permutation = JS_GetPropertyStr(ctx, event, "permutationToPlace");
                BlockState read;
                if (api.readPermutation(permutation, read))
                    chosen = read;
                JS_FreeValue(ctx, permutation);
            });

    if (!cancelled)
        placed = chosen;
    return cancelled;
}

void ScriptEngine::onCustomBlockPlaced(Level &level, const Vector3i &position, const BlockState &placed,
                                       const BlockState &previous) {
    const CustomBlockDefinition *block = CustomContentRegistry::getInstance().findBlock(placed.mName);
    if (block == nullptr)
        return;

    // once for the server, not per pack: every pack sees the same custom block ticking
    _scheduleBlockTick(level, placed.mName, position);
    _fireComponent(true, CustomContentRegistry::componentsFor(*block, placed.mStates), "onPlace",
                   "BlockComponentOnPlaceEvent", [&](ScriptApi &api, JSValue event) {
                       api.fillBlockEvent(event, level, position);
                       JS_SetPropertyStr(api.context(), event, "previousBlock", api.makePermutation(previous));
                   });
}

void ScriptEngine::onCustomBlockStateChange(Level &level, const Vector3i &position, const BlockState &now,
                                            const BlockState &previous) {
    const CustomBlockDefinition *block = CustomContentRegistry::getInstance().findBlock(now.mName);
    if (block == nullptr)
        return;

    _fireComponent(true, CustomContentRegistry::componentsFor(*block, now.mStates), "onBlockStateChange",
                   "BlockComponentBlockStateChangeEvent", [&](ScriptApi &api, JSValue event) {
                       api.fillBlockEvent(event, level, position);
                       JS_SetPropertyStr(api.context(), event, "previousPermutation", api.makePermutation(previous));
                   });
}

void ScriptEngine::onCustomBlockBreak(Level &level, const Vector3i &position, const BlockState &broken,
                                      const Actor *entitySource, const Vector3i *blockSource) {
    const CustomBlockDefinition *block = CustomContentRegistry::getInstance().findBlock(broken.mName);
    if (block == nullptr)
        return;

    mRedstonePower[&level].erase(RedstoneSystem::packPosition(position));
    _fireComponent(true, CustomContentRegistry::componentsFor(*block, broken.mStates), "onBreak",
                   "BlockComponentBlockBreakEvent", [&](ScriptApi &api, JSValue event) {
                       JSContext *ctx = api.context();
                       api.fillBlockEvent(event, level, position);
                       JS_SetPropertyStr(ctx, event, "brokenBlockPermutation", api.makePermutation(broken));
                       if (entitySource != nullptr)
                           JS_SetPropertyStr(ctx, event, "entitySource",
                                             api.makeEntity(const_cast<Actor &>(*entitySource)));
                       if (blockSource != nullptr)
                           JS_SetPropertyStr(ctx, event, "blockDestructionSource",
                                             api.makeBlock(level, blockSource->x, blockSource->y, blockSource->z));
                   });
}

void ScriptEngine::onItemUse(ServerPlayer &player) {
    const CustomItemDefinition *item = heldCustomItem(player);
    if (item == nullptr)
        return;

    _fireComponent(false, item->mCustomComponents, "onUse", "ItemComponentUseEvent",
                   [&](ScriptApi &api, JSValue event) {
                       JS_SetPropertyStr(api.context(), event, "source", api.makePlayer(player));
                       JS_SetPropertyStr(api.context(), event, "itemStack",
                                         api.makeItem(player.getInventory().getItemInHand()));
                   });
}

namespace {
    // EntityItemPickupEventOptions: entityFilter through Entity.matches, itemFilter.includeTypes by type id
    bool acceptsPickup(ScriptApi &api, JSValueConst options, JSValueConst entity, const ItemStack &picked) {
        JSContext *ctx = api.context();
        JSValue entityFilter = JS_GetPropertyStr(ctx, options, "entityFilter");
        bool accepted = true;
        if (JS_IsObject(entityFilter)) {
            JSValue matches = JS_GetPropertyStr(ctx, entity, "matches");
            JSValue result = JS_IsFunction(ctx, matches) ? JS_Call(ctx, matches, entity, 1, &entityFilter)
                                                         : JS_UNDEFINED;
            if (JS_IsException(result))
                JS_FreeValue(ctx, JS_GetException(ctx));
            accepted = JS_ToBool(ctx, result) == 1;
            JS_FreeValue(ctx, result);
            JS_FreeValue(ctx, matches);
        }
        JS_FreeValue(ctx, entityFilter);

        JSValue itemFilter = JS_GetPropertyStr(ctx, options, "itemFilter");
        JSValue types = JS_IsObject(itemFilter) ? JS_GetPropertyStr(ctx, itemFilter, "includeTypes") : JS_UNDEFINED;
        if (accepted && JS_IsArray(types) && picked.mDefinition != nullptr) {
            uint32_t length = 0;
            JSValue lengthValue = JS_GetPropertyStr(ctx, types, "length");
            JS_ToUint32(ctx, &length, lengthValue);
            JS_FreeValue(ctx, lengthValue);
            bool found = false;
            for (uint32_t index = 0; index < length && !found; ++index) {
                JSValue type = JS_GetPropertyUint32(ctx, types, index);
                JSValue id = JS_IsObject(type) ? JS_GetPropertyStr(ctx, type, "id") : JS_DupValue(ctx, type);
                std::string name = toString(ctx, id);
                if (name.find(':') == std::string::npos)
                    name = "minecraft:" + name;
                found = name == picked.mDefinition->getIdentifier();
                JS_FreeValue(ctx, id);
                JS_FreeValue(ctx, type);
            }
            accepted = found;
        }
        JS_FreeValue(ctx, types);
        JS_FreeValue(ctx, itemFilter);
        return accepted;
    }
}

namespace {
    // Entity.matches against an EntityFilter; a missing filter matches
    bool entityMatches(JSContext *ctx, JSValueConst entity, JSValueConst filter) {
        if (!JS_IsObject(filter))
            return true;
        JSValue matches = JS_GetPropertyStr(ctx, entity, "matches");
        JSValue result = JS_IsFunction(ctx, matches) ? JS_Call(ctx, matches, entity, 1, &filter) : JS_UNDEFINED;
        if (JS_IsException(result))
            JS_FreeValue(ctx, JS_GetException(ctx));
        const bool matched = JS_ToBool(ctx, result) == 1;
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, matches);
        return matched;
    }

    // EntityContainerAccessEventOptions: entityFilter on the container, accessSourceFilter on who opened it
    bool acceptsContainerAccess(JSContext *ctx, JSValueConst options, JSValueConst container, JSValueConst source) {
        JSValue entityFilter = JS_GetPropertyStr(ctx, options, "entityFilter");
        bool accepted = entityMatches(ctx, container, entityFilter);
        JS_FreeValue(ctx, entityFilter);
        JSValue sourceFilter = JS_GetPropertyStr(ctx, options, "accessSourceFilter");
        if (accepted && JS_IsObject(sourceFilter)) {
            JSValue filter = JS_GetPropertyStr(ctx, sourceFilter, "entityFilter");
            accepted = entityMatches(ctx, source, filter);
            JS_FreeValue(ctx, filter);
        }
        JS_FreeValue(ctx, sourceFilter);
        return accepted;
    }
}

void ScriptEngine::_emitContainerAccess(const char *key, const char *signal, const char *sourceField, Actor &container,
                                        Actor &source) {
    for (std::unique_ptr<ScriptApi> &api: mApis) {
        if (!api->hasSubscribers(key))
            continue;
        _arm();
        JSContext *ctx = api->context();
        JSValue event = api->makeEvent(signal);
        JSValue entity = api->makeEntity(container);
        JSValue opener = api->makeEntity(source);
        JS_SetPropertyStr(ctx, event, "entity", JS_DupValue(ctx, entity));
        JSValue access = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, access, "entity", JS_DupValue(ctx, opener));
        JS_SetPropertyStr(ctx, event, sourceField, access);
        api->emitFiltered(key, event,
                          [&](JSValueConst options) { return acceptsContainerAccess(ctx, options, entity, opener); });
        JS_FreeValue(ctx, opener);
        JS_FreeValue(ctx, entity);
        _pumpJobs();
    }
}

void ScriptEngine::onEntityContainerOpened(Actor &container, Actor &source) {
    _emitContainerAccess("after.entityContainerOpened", "WorldAfterEvents.entityContainerOpened", "openSource",
                         container, source);
}

void ScriptEngine::onEntityContainerClosed(Actor &container, Actor &source) {
    _emitContainerAccess("after.entityContainerClosed", "WorldAfterEvents.entityContainerClosed", "closeSource",
                         container, source);
}

void ScriptEngine::onItemStartUse(ServerPlayer &player, const ItemStack &used, int32_t useDuration) {
    if (used.isAir())
        return;
    _emit("after.itemStartUse", "WorldAfterEvents.itemStartUse", false, [&](ScriptApi &api, JSValue event) {
        JSContext *ctx = api.context();
        JS_SetPropertyStr(ctx, event, "itemStack", api.makeItem(used));
        JS_SetPropertyStr(ctx, event, "source", api.makePlayer(player));
        JS_SetPropertyStr(ctx, event, "useDuration", JS_NewInt32(ctx, useDuration));
    });
}

void ScriptEngine::onItemStopUse(ServerPlayer &player, const ItemStack &used, int32_t useDuration, bool released) {
    const auto fill = [&](ScriptApi &api, JSValue event) {
        JSContext *ctx = api.context();
        JS_SetPropertyStr(ctx, event, "itemStack", api.makeItem(used));
        JS_SetPropertyStr(ctx, event, "source", api.makePlayer(player));
        JS_SetPropertyStr(ctx, event, "useDuration", JS_NewInt32(ctx, useDuration < 0 ? 0 : useDuration));
    };
    if (released)
        _emit("after.itemReleaseUse", "WorldAfterEvents.itemReleaseUse", false, fill);
    _emit("after.itemStopUse", "WorldAfterEvents.itemStopUse", false, fill);
}

bool ScriptEngine::beforeEntityItemPickup(Actor &collector, Actor &item, const ItemStack &picked) {
    bool cancelled = false;
    for (std::unique_ptr<ScriptApi> &api: mApis) {
        if (!api->hasSubscribers("before.entityItemPickup"))
            continue;
        _arm();
        JSContext *ctx = api->context();
        JSValue event = api->makeEvent("WorldBeforeEvents.entityItemPickup");
        JSValue entity = api->makeEntity(collector);
        JS_SetPropertyStr(ctx, event, "entity", JS_DupValue(ctx, entity));
        JS_SetPropertyStr(ctx, event, "item", api->makeEntity(item));
        JS_SetPropertyStr(ctx, event, "cancel", JS_FALSE);
        ScriptApi &target = *api;
        cancelled = api->emitFiltered("before.entityItemPickup", event, [&](JSValueConst options) {
            return acceptsPickup(target, options, entity, picked);
        }) || cancelled;
        JS_FreeValue(ctx, entity);
        _pumpJobs();
    }
    return cancelled;
}

void ScriptEngine::onEntityItemPickup(Actor &collector, const ItemStack &picked) {
    for (std::unique_ptr<ScriptApi> &api: mApis) {
        if (!api->hasSubscribers("after.entityItemPickup"))
            continue;
        _arm();
        JSContext *ctx = api->context();
        JSValue event = api->makeEvent("WorldAfterEvents.entityItemPickup");
        JSValue entity = api->makeEntity(collector);
        JS_SetPropertyStr(ctx, event, "entity", JS_DupValue(ctx, entity));
        JSValue items = JS_NewArray(ctx);
        JS_SetPropertyUint32(ctx, items, 0, api->makeItem(picked));
        JS_SetPropertyStr(ctx, event, "items", items);
        ScriptApi &target = *api;
        api->emitFiltered("after.entityItemPickup", event,
                          [&](JSValueConst options) { return acceptsPickup(target, options, entity, picked); });
        JS_FreeValue(ctx, entity);
        _pumpJobs();
    }
}

void ScriptEngine::onItemCompleteUse(ServerPlayer &player, const ItemStack &used, int32_t useDuration) {
    if (used.isAir() || used.mDefinition == nullptr)
        return;
    _emit("after.itemCompleteUse", "WorldAfterEvents.itemCompleteUse", false, [&](ScriptApi &api, JSValue event) {
        JSContext *ctx = api.context();
        JS_SetPropertyStr(ctx, event, "itemStack", api.makeItem(used));
        JS_SetPropertyStr(ctx, event, "source", api.makePlayer(player));
        JS_SetPropertyStr(ctx, event, "useDuration", JS_NewInt32(ctx, useDuration));
    });
    const CustomItemDefinition *item = CustomContentRegistry::getInstance().findItem(used.mDefinition->getIdentifier());
    if (item == nullptr)
        return;

    const auto fill = [&](ScriptApi &api, JSValue event) {
        JS_SetPropertyStr(api.context(), event, "source", api.makePlayer(player));
        JS_SetPropertyStr(api.context(), event, "itemStack", api.makeItem(used));
    };
    _fireComponent(false, item->mCustomComponents, "onCompleteUse", "ItemComponentCompleteUseEvent", fill);
}

void ScriptEngine::onItemConsume(ServerPlayer &player, const ItemStack &used) {
    if (used.isAir() || used.mDefinition == nullptr)
        return;
    const CustomItemDefinition *item = CustomContentRegistry::getInstance().findItem(used.mDefinition->getIdentifier());
    // the game subscribes onConsume to the food component's signal only (sub_14A7F6070)
    if (item == nullptr || !item->mIsFood)
        return;

    _fireComponent(false, item->mCustomComponents, "onConsume", "ItemComponentConsumeEvent",
                   [&](ScriptApi &api, JSValue event) {
                       JS_SetPropertyStr(api.context(), event, "source", api.makePlayer(player));
                       JS_SetPropertyStr(api.context(), event, "itemStack", api.makeItem(used));
                   });
}

int32_t ScriptEngine::onItemHitEntity(ServerPlayer &attacker, Actor &target, bool hadEffect, int32_t durabilityDamage) {
    const CustomItemDefinition *item = heldCustomItem(attacker);
    if (item == nullptr)
        return durabilityDamage;

    const ItemStack held = attacker.getInventory().getItemInHand();
    // before the hit: item custom component after events wait for ScriptDeferredEventCoordinator's flush,
    // onBeforeDurabilityDamage runs in place to read durabilityDamage back
    _fireComponent(
            false, item->mCustomComponents, "onBeforeDurabilityDamage", "ItemComponentBeforeDurabilityDamageEvent",
            [&](ScriptApi &api, JSValue event) {
                JSContext *ctx = api.context();
                JS_SetPropertyStr(ctx, event, "attackingEntity", api.makePlayer(attacker));
                JS_SetPropertyStr(ctx, event, "hitEntity", api.makeEntity(target));
                JS_SetPropertyStr(ctx, event, "itemStack", api.makeItem(held));
                JS_SetPropertyStr(ctx, event, "durabilityDamage", JS_NewInt32(ctx, durabilityDamage));
            },
            [&](ScriptApi &api, JSValue event) {
                JSContext *ctx = api.context();
                JSValue value = JS_GetPropertyStr(ctx, event, "durabilityDamage");
                double damage = 0.0;
                if (JS_ToFloat64(ctx, &damage, value) == 0 && std::isfinite(damage) && damage >= 0.0
                    && damage <= INT32_MAX)
                    durabilityDamage = (int32_t) damage;
                JS_FreeValue(ctx, value);
            });
    _fireComponent(false, item->mCustomComponents, "onHitEntity", "ItemComponentHitEntityEvent",
                   [&](ScriptApi &api, JSValue event) {
                       JSContext *ctx = api.context();
                       JS_SetPropertyStr(ctx, event, "attackingEntity", api.makePlayer(attacker));
                       JS_SetPropertyStr(ctx, event, "hitEntity", api.makeEntity(target));
                       JS_SetPropertyStr(ctx, event, "hadEffect", JS_NewBool(ctx, hadEffect));
                       JS_SetPropertyStr(ctx, event, "itemStack", api.makeItem(held));
                   });

    return durabilityDamage;
}

void ScriptEngine::onEntitySpawn(ServerActor &entity, const char *cause) {
    _emit("after.entitySpawn", "WorldAfterEvents.entitySpawn", false, [&](ScriptApi &api, JSValue event) {
        JS_SetPropertyStr(api.context(), event, "entity", api.makeActor(entity));
        JS_SetPropertyStr(api.context(), event, "cause", JS_NewString(api.context(), cause));
    });
}

void ScriptEngine::beforeEntityRemove(ServerActor &entity) {
    _emit("before.entityRemove", "WorldBeforeEvents.entityRemove", false, [&](ScriptApi &api, JSValue event) {
        JS_SetPropertyStr(api.context(), event, "removedEntity", api.makeActor(entity));
    });
}

void ScriptEngine::onEntityRemove(ServerActor &entity) {
    beforeEntityRemove(entity);
    _emit("after.entityRemove", "WorldAfterEvents.entityRemove", false, [&](ScriptApi &api, JSValue event) {
        JSContext *ctx = api.context();
        JS_SetPropertyStr(ctx, event, "removedEntityId", string(ctx, std::to_string(entity.getUniqueId())));
        JS_SetPropertyStr(ctx, event, "typeId", string(ctx, entity.getTypeId()));
    });

    for (std::unique_ptr<ScriptApi> &api: mApis)
        api->forgetEntity(entity.getRuntimeId());
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

bool ScriptEngine::beforeEntityHurt(Actor &hurtEntity, float &damage, const ActorDamageSource &source) {
    bool cancelled = false;
    for (std::unique_ptr<ScriptApi> &api: mApis) {
        if (!api->hasSubscribers("before.entityHurt"))
            continue;

        _arm();
        JSContext *ctx = api->context();
        JSValue event = api->makeEvent("WorldBeforeEvents.entityHurt");
        JS_SetPropertyStr(ctx, event, "hurtEntity", api->makeEntity(hurtEntity));
        JS_SetPropertyStr(ctx, event, "damage", JS_NewFloat64(ctx, damage));
        JS_SetPropertyStr(ctx, event, "damageSource", api->makeDamageSource(source));
        JS_SetPropertyStr(ctx, event, "cancel", JS_FALSE);
        JS_DupValue(ctx, event);
        cancelled = api->emit("before.entityHurt", event) || cancelled;

        JSValue value = JS_GetPropertyStr(ctx, event, "damage");
        double rewritten = damage;
        if (JS_ToFloat64(ctx, &rewritten, value) == 0 && std::isfinite(rewritten))
            damage = (float) rewritten;
        JS_FreeValue(ctx, value);
        JS_FreeValue(ctx, event);
        _pumpJobs();
    }
    return cancelled;
}

void ScriptEngine::onEntityHurt(Actor &hurtEntity, float damage, const ActorDamageSource &source) {
    _emit("after.entityHurt", "WorldAfterEvents.entityHurt", false, [&](ScriptApi &api, JSValue event) {
        JSContext *ctx = api.context();
        JS_SetPropertyStr(ctx, event, "hurtEntity", api.makeEntity(hurtEntity));
        JS_SetPropertyStr(ctx, event, "damage", JS_NewFloat64(ctx, damage));
        JS_SetPropertyStr(ctx, event, "damageSource", api.makeDamageSource(source));
    });
}

void ScriptEngine::onEntityDie(Actor &deadEntity, const ActorDamageSource &source) {
    _emit("after.entityDie", "WorldAfterEvents.entityDie", false, [&](ScriptApi &api, JSValue event) {
        JS_SetPropertyStr(api.context(), event, "deadEntity", api.makeEntity(deadEntity));
        JS_SetPropertyStr(api.context(), event, "damageSource", api.makeDamageSource(source));
    });
}

bool ScriptEngine::beforeEffectAdd(Actor &entity, MobEffectInstance &effect) {
    bool cancelled = false;
    for (std::unique_ptr<ScriptApi> &api: mApis) {
        if (!api->hasSubscribers("before.effectAdd"))
            continue;

        _arm();
        JSContext *ctx = api->context();
        JSValue event = api->makeEvent("WorldBeforeEvents.effectAdd");
        JS_SetPropertyStr(ctx, event, "entity", api->makeEntity(entity));
        JS_SetPropertyStr(ctx, event, "effectType", string(ctx, ScriptApi::effectTypeId(effect.mId)));
        JS_SetPropertyStr(ctx, event, "duration", JS_NewInt32(ctx, effect.mDuration));
        JS_SetPropertyStr(ctx, event, "cancel", JS_FALSE);
        JS_DupValue(ctx, event);
        cancelled = api->emit("before.effectAdd", event) || cancelled;

        JSValue value = JS_GetPropertyStr(ctx, event, "duration");
        int32_t duration = effect.mDuration;
        if (JS_ToInt32(ctx, &duration, value) == 0)
            effect.mDuration = duration;
        JS_FreeValue(ctx, value);
        JS_FreeValue(ctx, event);
        _pumpJobs();
    }
    return cancelled;
}

void ScriptEngine::onEffectAdd(Actor &entity, const MobEffectInstance &effect) {
    _emit("after.effectAdd", "WorldAfterEvents.effectAdd", false, [&](ScriptApi &api, JSValue event) {
        JS_SetPropertyStr(api.context(), event, "entity", api.makeEntity(entity));
        JS_SetPropertyStr(api.context(), event, "effect", api.makeEffect(entity, effect.mId));
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

void ScriptEngine::onPlayerInteractWithEntity(ServerPlayer &player, Actor &target, const ItemStack &before) {
    _emit("after.playerInteractWithEntity", "WorldAfterEvents.playerInteractWithEntity", false,
          [&](ScriptApi &api, JSValue event) {
              JSContext *ctx = api.context();
              JS_SetPropertyStr(ctx, event, "player", api.makePlayer(player));
              JS_SetPropertyStr(ctx, event, "target", api.makeEntity(target));
              JS_SetPropertyStr(ctx, event, "beforeItemStack", api.makeItem(before));
              JS_SetPropertyStr(ctx, event, "itemStack", api.makeItem(player.getInventory().getItemInHand()));
          });
}

void ScriptEngine::onEntitySneak(Actor &entity, bool started) {
    const char *key = started ? "after.entityStartSneaking" : "after.entityStopSneaking";
    const char *signal = started ? "WorldAfterEvents.entityStartSneaking" : "WorldAfterEvents.entityStopSneaking";
    _emit(key, signal, false, [&](ScriptApi &api, JSValue event) {
        JS_SetPropertyStr(api.context(), event, "entity", api.makeEntity(entity));
    });
}

bool ScriptEngine::beforeEntityTamed(Actor &entity, Actor &tamer) {
    return _emit("before.entityTamed", "WorldBeforeEvents.entityTamed", true, [&](ScriptApi &api, JSValue event) {
        JS_SetPropertyStr(api.context(), event, "entity", api.makeEntity(entity));
        JS_SetPropertyStr(api.context(), event, "tamingEntity", api.makeEntity(tamer));
    });
}

void ScriptEngine::onEntityTamed(Actor &entity, Actor &tamer) {
    _emit("after.entityTamed", "WorldAfterEvents.entityTamed", false, [&](ScriptApi &api, JSValue event) {
        JS_SetPropertyStr(api.context(), event, "entity", api.makeEntity(entity));
        JS_SetPropertyStr(api.context(), event, "tamingEntity", api.makeEntity(tamer));
    });
}

void ScriptEngine::onDataDrivenEntityTrigger(MobActor &entity, const std::string &eventId,
                                             const std::vector<std::string> &added,
                                             const std::vector<std::string> &removed) {
    _emit("after.dataDrivenEntityTrigger", "WorldAfterEvents.dataDrivenEntityTrigger", false,
          [&](ScriptApi &api, JSValue event) {
              JSContext *ctx = api.context();
              JS_SetPropertyStr(ctx, event, "entity", api.makeActor(entity));
              JS_SetPropertyStr(ctx, event, "eventId", string(ctx, eventId));
              JS_SetPropertyStr(ctx, event, "getModifiers", api.makeModifiersGetter(added, removed));
          });
}

void ScriptEngine::onEntityUpgrade(MobActor &entity, const std::string &upgradeId,
                                   const std::vector<std::string> &added, const std::vector<std::string> &removed) {
    _emit("after.entityUpgrade", "WorldAfterEvents.entityUpgrade", false, [&](ScriptApi &api, JSValue event) {
        JSContext *ctx = api.context();
        JS_SetPropertyStr(ctx, event, "entity", api.makeActor(entity));
        JS_SetPropertyStr(ctx, event, "upgradeId", string(ctx, upgradeId));
        JS_SetPropertyStr(ctx, event, "getModifiers", api.makeModifiersGetter(added, removed));
    });
}

bool ScriptEngine::beforePlayerGameModeChange(ServerPlayer &player, int32_t fromGameMode, int32_t &toGameMode) {
    bool cancelled = false;
    for (std::unique_ptr<ScriptApi> &api: mApis) {
        if (!api->hasSubscribers("before.playerGameModeChange"))
            continue;

        _arm();
        JSContext *ctx = api->context();
        JSValue event = api->makeEvent("WorldBeforeEvents.playerGameModeChange");
        JS_SetPropertyStr(ctx, event, "player", api->makePlayer(player));
        JS_SetPropertyStr(ctx, event, "fromGameMode", JS_NewString(ctx, api->gameModeName(fromGameMode)));
        JS_SetPropertyStr(ctx, event, "toGameMode", JS_NewString(ctx, api->gameModeName(toGameMode)));
        JS_SetPropertyStr(ctx, event, "cancel", JS_FALSE);
        JS_DupValue(ctx, event);
        cancelled = api->emit("before.playerGameModeChange", event) || cancelled;

        JSValue value = JS_GetPropertyStr(ctx, event, "toGameMode");
        const int32_t parsed = api->parseGameMode(toString(ctx, value));
        if (parsed >= 0)
            toGameMode = parsed;
        JS_FreeValue(ctx, value);
        JS_FreeValue(ctx, event);
        _pumpJobs();
    }
    return cancelled;
}

void ScriptEngine::onPlayerGameModeChange(ServerPlayer &player, int32_t fromGameMode, int32_t toGameMode) {
    _emit("after.playerGameModeChange", "WorldAfterEvents.playerGameModeChange", false,
          [&](ScriptApi &api, JSValue event) {
              JSContext *ctx = api.context();
              JS_SetPropertyStr(ctx, event, "player", api.makePlayer(player));
              JS_SetPropertyStr(ctx, event, "fromGameMode", JS_NewString(ctx, api.gameModeName(fromGameMode)));
              JS_SetPropertyStr(ctx, event, "toGameMode", JS_NewString(ctx, api.gameModeName(toGameMode)));
          });
}

void ScriptEngine::onPlayerDimensionChange(ServerPlayer &player, const Level &from, const Vector3f &fromLocation,
                                           const Level &to, const Vector3f &toLocation) {
    _emit("after.playerDimensionChange", "WorldAfterEvents.playerDimensionChange", false,
          [&](ScriptApi &api, JSValue event) {
              JSContext *ctx = api.context();
              JS_SetPropertyStr(ctx, event, "player", api.makePlayer(player));
              JS_SetPropertyStr(ctx, event, "fromDimension", api.makeDimension(from));
              JS_SetPropertyStr(ctx, event, "fromLocation",
                                vector3(ctx, fromLocation.x, fromLocation.y, fromLocation.z));
              JS_SetPropertyStr(ctx, event, "toDimension", api.makeDimension(to));
              JS_SetPropertyStr(ctx, event, "toLocation", vector3(ctx, toLocation.x, toLocation.y, toLocation.z));
          });
}

void ScriptEngine::onPlayerEmote(ServerPlayer &player, const std::string &personaPieceId) {
    _emit("after.playerEmote", "WorldAfterEvents.playerEmote", false, [&](ScriptApi &api, JSValue event) {
        JS_SetPropertyStr(api.context(), event, "player", api.makePlayer(player));
        JS_SetPropertyStr(api.context(), event, "personaPieceId", string(api.context(), personaPieceId));
    });
}

void ScriptEngine::onPlayerInputModeChange(ServerPlayer &player, const char *previous, const char *current) {
    _emit("after.playerInputModeChange", "WorldAfterEvents.playerInputModeChange", false,
          [&](ScriptApi &api, JSValue event) {
              JSContext *ctx = api.context();
              JS_SetPropertyStr(ctx, event, "player", api.makePlayer(player));
              JS_SetPropertyStr(ctx, event, "previousInputModeUsed", JS_NewString(ctx, previous));
              JS_SetPropertyStr(ctx, event, "newInputModeUsed", JS_NewString(ctx, current));
          });
}

void ScriptEngine::onPlayerButtonInput(ServerPlayer &player, const char *button, bool pressed) {
    _emit("after.playerButtonInput", "WorldAfterEvents.playerButtonInput", false, [&](ScriptApi &api, JSValue event) {
        JSContext *ctx = api.context();
        JS_SetPropertyStr(ctx, event, "player", api.makePlayer(player));
        JS_SetPropertyStr(ctx, event, "button", JS_NewString(ctx, button));
        JS_SetPropertyStr(ctx, event, "newButtonState", JS_NewString(ctx, pressed ? "Pressed" : "Released"));
    });
}

void ScriptEngine::onPlayerSwingStart(ServerPlayer &player, const char *source) {
    _emit("after.playerSwingStart", "WorldAfterEvents.playerSwingStart", false, [&](ScriptApi &api, JSValue event) {
        JSContext *ctx = api.context();
        JS_SetPropertyStr(ctx, event, "player", api.makePlayer(player));
        JS_SetPropertyStr(ctx, event, "heldItemStack", api.makeItem(player.getInventory().getItemInHand()));
        JS_SetPropertyStr(ctx, event, "swingSource", JS_NewString(ctx, source));
    });
}

void ScriptEngine::beforePlayerLeave(ServerPlayer &player) {
    _emit("before.playerLeave", "WorldBeforeEvents.playerLeave", false, [&](ScriptApi &api, JSValue event) {
        JS_SetPropertyStr(api.context(), event, "player", api.makePlayer(player));
    });
}

namespace {
    const char *const HEAL_CAUSES[] = {"Heal", "Regeneration", "SelfHeal", "TotemOfUndying"};
}

bool ScriptEngine::_beforeHeal(Actor &entity, float &amount, int32_t cause) {
    bool cancelled = false;
    for (std::unique_ptr<ScriptApi> &api: mApis) {
        if (!api->hasSubscribers("before.entityHeal"))
            continue;

        _arm();
        JSContext *ctx = api->context();
        JSValue event = api->makeEvent("WorldBeforeEvents.entityHeal");
        JS_SetPropertyStr(ctx, event, "healedEntity", api->makeEntity(entity));
        JS_SetPropertyStr(ctx, event, "healing", JS_NewFloat64(ctx, amount));
        JS_SetPropertyStr(ctx, event, "healSource", api->makeHealSource(HEAL_CAUSES[cause]));
        JS_SetPropertyStr(ctx, event, "cancel", JS_FALSE);
        JS_DupValue(ctx, event);
        cancelled = api->emit("before.entityHeal", event) || cancelled;

        JSValue value = JS_GetPropertyStr(ctx, event, "healing");
        double healing = amount;
        if (JS_ToFloat64(ctx, &healing, value) == 0 && std::isfinite(healing))
            amount = (float) healing;
        JS_FreeValue(ctx, value);
        JS_FreeValue(ctx, event);
        _pumpJobs();
    }
    return cancelled;
}

void ScriptEngine::_onHeal(Actor &entity, float amount, int32_t cause) {
    _emit("after.entityHeal", "WorldAfterEvents.entityHeal", false, [&](ScriptApi &api, JSValue event) {
        JSContext *ctx = api.context();
        JS_SetPropertyStr(ctx, event, "healedEntity", api.makeEntity(entity));
        JS_SetPropertyStr(ctx, event, "healing", JS_NewFloat64(ctx, amount));
        JS_SetPropertyStr(ctx, event, "healSource", api.makeHealSource(HEAL_CAUSES[cause]));
    });
}

void ScriptEngine::_onHealthChanged(Actor &entity, float oldValue, float newValue) {
    _emit("after.entityHealthChanged", "WorldAfterEvents.entityHealthChanged", false,
          [&](ScriptApi &api, JSValue event) {
              JSContext *ctx = api.context();
              JS_SetPropertyStr(ctx, event, "entity", api.makeEntity(entity));
              JS_SetPropertyStr(ctx, event, "oldValue", JS_NewFloat64(ctx, oldValue));
              JS_SetPropertyStr(ctx, event, "newValue", JS_NewFloat64(ctx, newValue));
          });
}

void ScriptEngine::onGameRuleChange(const std::string &rule, bool isBool, bool boolValue, int32_t intValue) {
    for (std::unique_ptr<ScriptApi> &api: mApis) {
        if (!api->hasSubscribers("after.gameRuleChange"))
            continue;

        std::string official;
        for (const auto &value: api->surface().enumValues("GameRule")) {
            std::string lower = value.second;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower == rule)
                official = value.second;
        }
        if (official.empty())
            continue;

        _arm();
        JSContext *ctx = api->context();
        JSValue event = api->makeEvent("WorldAfterEvents.gameRuleChange");
        JS_SetPropertyStr(ctx, event, "rule", string(ctx, official));
        JS_SetPropertyStr(ctx, event, "value", isBool ? JS_NewBool(ctx, boolValue) : JS_NewInt32(ctx, intValue));
        api->emit("after.gameRuleChange", event);
        _pumpJobs();
    }
}

bool ScriptEngine::beforeWeatherChange(const std::string &previousWeather, std::string &newWeather, int32_t &duration) {
    bool cancelled = false;
    for (std::unique_ptr<ScriptApi> &api: mApis) {
        if (!api->hasSubscribers("before.weatherChange"))
            continue;

        _arm();
        JSContext *ctx = api->context();
        JSValue event = api->makeEvent("WorldBeforeEvents.weatherChange");
        JS_SetPropertyStr(ctx, event, "cancel", JS_FALSE);
        JS_SetPropertyStr(ctx, event, "duration", JS_NewInt32(ctx, duration));
        JS_SetPropertyStr(ctx, event, "newWeather", string(ctx, newWeather));
        JS_SetPropertyStr(ctx, event, "previousWeather", string(ctx, previousWeather));

        // emit frees the event: keep it to read back what the subscribers changed
        JS_DupValue(ctx, event);
        cancelled = api->emit("before.weatherChange", event) || cancelled;

        JSValue weatherValue = JS_GetPropertyStr(ctx, event, "newWeather");
        const std::string weather = toString(ctx, weatherValue);
        JS_FreeValue(ctx, weatherValue);
        if (weather == "Clear" || weather == "Rain" || weather == "Thunder")
            newWeather = weather;

        JSValue durationValue = JS_GetPropertyStr(ctx, event, "duration");
        double ticks = 0;
        if (JS_ToFloat64(ctx, &ticks, durationValue) == 0 && std::isfinite(ticks) && ticks >= 1 && ticks <= INT32_MAX)
            duration = (int32_t) ticks;
        JS_FreeValue(ctx, durationValue);
        JS_FreeValue(ctx, event);
        _pumpJobs();
    }
    return cancelled;
}

void ScriptEngine::onWeatherChange(const std::string &dimension, const std::string &previousWeather,
                                   const std::string &newWeather) {
    _emit("after.weatherChange", "WorldAfterEvents.weatherChange", false, [&](ScriptApi &api, JSValue event) {
        JSContext *ctx = api.context();
        JS_SetPropertyStr(ctx, event, "dimension", string(ctx, dimension));
        JS_SetPropertyStr(ctx, event, "newWeather", string(ctx, newWeather));
        JS_SetPropertyStr(ctx, event, "previousWeather", string(ctx, previousWeather));
    });
}

void ScriptEngine::emitScriptEvent(const std::string &id, const std::string &message, const ScriptEventOrigin &origin) {
    _emit(ScriptApi::SCRIPT_EVENT_KEY, "SystemAfterEvents.scriptEventReceive", false,
          [&](ScriptApi &api, JSValue event) {
              JSContext *ctx = api.context();
              JS_SetPropertyStr(ctx, event, "id", string(ctx, id));
              JS_SetPropertyStr(ctx, event, "message", string(ctx, message));
              JS_SetPropertyStr(ctx, event, "sourceType", string(ctx, origin.mSourceType));
              if (origin.mSourceEntity != nullptr)
                  JS_SetPropertyStr(ctx, event, "sourceEntity", api.makeEntity(*origin.mSourceEntity));
              if (origin.mInitiator != nullptr)
                  JS_SetPropertyStr(ctx, event, "initiator", api.makeEntity(*origin.mInitiator));
              if (origin.mBlockLevel != nullptr)
                  JS_SetPropertyStr(ctx, event, "sourceBlock",
                                    api.makeBlock(*origin.mBlockLevel, origin.mBlockPosition.x, origin.mBlockPosition.y,
                                                  origin.mBlockPosition.z));
          });
}

void ScriptEngine::queueScriptEvent(const std::string &id, const std::string &message) {
    mQueuedScriptEvents.emplace_back(id, message);
}

void ScriptEngine::tick(int64_t currentTick) {
    std::vector<std::pair<std::string, std::string>> queued;
    queued.swap(mQueuedScriptEvents);
    for (const auto &event: queued)
        emitScriptEvent(event.first, event.second);

    // sub_145D0A460: every clock stands still without the daylight cycle
    if (mHost == nullptr || mHost->getWorlds().getDefault().getOverworld().getGameRules().getBool("dodaylightcycle"))
        mClocks.tick();
    if (currentTick - mLastClockSync >= WorldClockSystem::SYNC_INTERVAL_TICKS) {
        mLastClockSync = currentTick;
        SyncWorldClocksPacket packet;
        mClocks.writeStates(packet);
        _broadcastClocks(packet);
    }

    for (std::unique_ptr<ScriptApi> &api: mApis) {
        _arm();
        api->tick(currentTick);
    }
    _pumpJobs();
}
