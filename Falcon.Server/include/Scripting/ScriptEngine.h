#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <memory>
#include <string>

struct JSRuntime;
struct JSContext;
struct MobEffectInstance;
class Actor;
class ItemStack;
class ScriptApi;
class ServerNetworkHandler;
class ServerActor;
class ServerPlayer;

class ScriptEngine {
public:
    static const int64_t DEFAULT_WATCHDOG_MS = 2000;

    ScriptEngine();

    ~ScriptEngine();

    void bindHost(ServerNetworkHandler &host);

    ScriptEngine(const ScriptEngine &) = delete;

    ScriptEngine &operator=(const ScriptEngine &) = delete;

    bool isReady() const { return mContext != nullptr; }

    bool evaluate(const std::string &source, const std::string &name, bool asModule = false);

    bool evaluateFile(const std::string &path);

    void tick(int64_t currentTick);

    void onProjectileHitBlock(ServerActor &projectile, int32_t x, int32_t y, int32_t z);

    void onItemUseOnBlock(ServerPlayer &player, int32_t x, int32_t y, int32_t z);

    void onWorldInitialize();

    void onProjectileHitEntity(ServerActor &projectile, Actor &hitEntity, const Vector3f &location);

    void onEntityHitBlock(Actor &damagingEntity, const Vector3i &position, int32_t face);

    void onEntityHitEntity(Actor &damagingEntity, Actor &hitEntity);

    void onEntityItemDrop(Actor &entity, const ItemStack &item);

    void onPlayerInteractWithBlock(ServerPlayer &player, const Vector3i &position, int32_t face);

    void onEntitySpawn(ServerActor &entity);

    void onEntityRemove(ServerActor &entity);

    void onEntityLoad(ServerActor &entity);

    bool beforePlayerInteractWithBlock(ServerPlayer &player, const Vector3i &position, int32_t face);

    bool beforeEntityHurt(Actor &hurtEntity, float damage, const std::string &cause, const Actor *attacker);

    bool beforeEffectAdd(Actor &entity, const MobEffectInstance &effect);

    bool beforePlayerInteractWithEntity(ServerPlayer &player, Actor &target);

    bool beforeItemUse(ServerPlayer &player);

    void setWatchdogMs(int64_t milliseconds) { mWatchdogMs = milliseconds; }

    JSContext *context() { return mContext; }

    JSRuntime *runtime() { return mRuntime; }

private:
    void _installConsole();

    void _pumpJobs();

    template<typename Fill>
    bool _emit(const char *name, bool cancellable, Fill fill);

    bool _checkException(bool hadError, const std::string &name);

    friend int _scriptInterruptHandler(JSRuntime *runtime, void *opaque);

    JSRuntime *mRuntime;
    JSContext *mContext;
    std::unique_ptr<ScriptApi> mApi;
    int64_t mWatchdogMs;
    int64_t mExecutionDeadlineMs;
};
