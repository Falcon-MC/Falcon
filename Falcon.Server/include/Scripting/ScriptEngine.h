#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "Scripting/Binding/PlayerHandleRegistry.h"

#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

struct JSRuntime;
struct JSContext;
struct BehaviorPack;
struct MobEffectInstance;
class Actor;
class ItemActor;
class ItemStack;
class Level;
class ScriptApi;
class ServerNetworkHandler;
class ServerActor;
class ServerPlayer;

// one runtime shared by every pack, one context per pack: packs never see each other's globals or handlers
class ScriptEngine {
public:
    static const int64_t DEFAULT_WATCHDOG_MS = 2000;

    // caps for an untrusted behavior pack script's heap and native stack
    static const size_t SCRIPT_MEMORY_LIMIT_BYTES = 64 * 1024 * 1024;
    static const size_t SCRIPT_MAX_STACK_BYTES = 1024 * 1024;

    ScriptEngine();

    ~ScriptEngine();

    void bindHost(ServerNetworkHandler &host);

    ScriptEngine(const ScriptEngine &) = delete;

    ScriptEngine &operator=(const ScriptEngine &) = delete;

    bool isReady() const {
        return mRuntime != nullptr;
    }

    // runs each script pack's entry in early execution; returns the packs whose scripts started
    size_t loadPacks(const std::vector<BehaviorPack> &packs);

    // startup, then the end of early execution and worldLoad
    void startup();

    void shutdown();

    void tick(int64_t currentTick);

    void onProjectileHitBlock(ServerActor &projectile, int32_t x, int32_t y, int32_t z);

    void onItemUseOnBlock(ServerPlayer &player, int32_t x, int32_t y, int32_t z);

    void onProjectileHitEntity(ServerActor &projectile, Actor &hitEntity, const Vector3f &location);

    void onEntityHitBlock(Actor &damagingEntity, const Vector3i &position, int32_t face);

    void onEntityHitEntity(Actor &damagingEntity, Actor &hitEntity);

    void onEntityItemDrop(Actor &entity, ItemActor *dropped);

    void onPlayerInteractWithBlock(ServerPlayer &player, const std::string &blockId, const Vector3i &position,
                                   int32_t face);

    void onCustomBlockTick(Level &level, const std::string &blockId, const Vector3i &position);

    void onCustomBlockStep(Actor &actor, const Level &level, const std::string &blockId, const Vector3i &position);

    void onCustomBlockStepOff(Actor &actor, const Level &level);

    void onEntitySpawn(ServerActor &entity);

    void onEntityRemove(ServerActor &entity);

    void onEntityLoad(ServerActor &entity);

    bool beforePlayerInteractWithBlock(ServerPlayer &player, const Vector3i &position, int32_t face);

    bool beforeEntityHurt(Actor &hurtEntity, float damage, const std::string &cause, const Actor *attacker);

    bool beforeEffectAdd(Actor &entity, const MobEffectInstance &effect);

    bool beforePlayerInteractWithEntity(ServerPlayer &player, Actor &target);

    bool beforeItemUse(ServerPlayer &player);

    void setWatchdogMs(int64_t milliseconds) {
        mWatchdogMs = milliseconds;
    }

    JSRuntime *runtime() {
        return mRuntime;
    }

private:
    bool _loadPack(const BehaviorPack &pack);

    void _installConsole(JSContext *context);

    void _pumpJobs();

    void _arm();

    // calls fill(api, event) for each pack with subscribers; true when one cancelled
    template <typename Fill>
    bool _emit(const char *key, const char *signal, bool cancellable, Fill fill);

    void _scheduleBlockTick(Level &level, const std::string &blockId, const Vector3i &position);

    friend int _scriptInterruptHandler(JSRuntime *runtime, void *opaque);

    JSRuntime *mRuntime;
    ServerNetworkHandler *mHost = nullptr;
    uint32_t mNativeClassId = 0;
    PlayerHandleRegistry mPlayers;
    std::vector<std::unique_ptr<ScriptApi>> mApis;
    std::mt19937 mTickRandom{std::random_device{}()};
    int64_t mWatchdogMs;
    int64_t mExecutionDeadlineMs;
};
