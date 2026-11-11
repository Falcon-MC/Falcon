#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "Scripting/Binding/PlayerHandleRegistry.h"
#include "Scripting/WorldClockSystem.h"

#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct JSRuntime;
struct JSContext;
struct BehaviorPack;
struct MobEffectInstance;
struct CustomComponentUse;
class BlockState;
class Actor;
class ItemActor;
class ItemStack;
class Level;
class ScriptApi;
class SyncWorldClocksPacket;
class ServerNetworkHandler;
class ServerActor;
class ServerPlayer;

// who ran /scriptevent or sent the event; a ScriptEventSource value and its optional actors and block
struct ScriptEventOrigin {
    const char *mSourceType = "Server";
    Actor *mSourceEntity = nullptr;
    Actor *mInitiator = nullptr;
    const Level *mBlockLevel = nullptr;
    Vector3i mBlockPosition;
};

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

    // clickPosition: the face location, relative to the block's bottom north-west corner
    void onItemUseOnBlock(ServerPlayer &player, const Vector3i &position, int32_t face, const Vector3f &clickPosition);

    void onProjectileHitEntity(ServerActor &projectile, Actor &hitEntity, const Vector3f &location);

    void onEntityHitBlock(Actor &damagingEntity, const Vector3i &position, int32_t face);

    void onEntityHitEntity(Actor &damagingEntity, Actor &hitEntity);

    void onEntityItemDrop(Actor &entity, ItemActor *dropped);

    void onPlayerInteractWithBlock(ServerPlayer &player, const std::string &blockId, const Vector3i &position,
                                   int32_t face, const Vector3f &clickPosition);

    void onCustomBlockTick(Level &level, const std::string &blockId, const Vector3i &position);

    void onCustomBlockRandomTick(Level &level, const std::string &blockId, const Vector3i &position);

    void onCustomBlockStep(Actor &actor, const Level &level, const std::string &blockId, const Vector3i &position);

    void onCustomBlockStepOff(Actor &actor, const Level &level);

    void onCustomBlockFallOn(Actor &actor, Level &level, const std::string &blockId, const Vector3i &position,
                             float fallDistance);

    // the power the block receives; hooks fire on a change reaching minecraft:redstone_consumer's min_power
    void onCustomBlockRedstone(Level &level, const std::string &blockId, const Vector3i &position, int32_t power);

    // beforeOnPlayerPlace; true when cancelled, placed takes the permutation scripts chose
    bool beforeCustomBlockPlace(ServerPlayer &player, Level &level, const Vector3i &position, int32_t face,
                                BlockState &placed);

    // any placement; previous is the block it replaced
    void onCustomBlockPlaced(Level &level, const Vector3i &position, const BlockState &placed,
                             const BlockState &previous);

    // same block, other states
    void onCustomBlockStateChange(Level &level, const Vector3i &position, const BlockState &now,
                                  const BlockState &previous);

    // any destruction; at most one of entitySource and blockSource (a piston) is set
    void onCustomBlockBreak(Level &level, const Vector3i &position, const BlockState &broken, const Actor *entitySource,
                            const Vector3i *blockSource = nullptr);

    void onItemUse(ServerPlayer &player);

    // the used stack, read before the game decrements it
    void onItemCompleteUse(ServerPlayer &player, const ItemStack &used);

    // food only, after onCompleteUse: the food component fires it once eaten
    void onItemConsume(ServerPlayer &player, const ItemStack &used);

    // onBeforeDurabilityDamage then onHitEntity; returns the durability damage scripts settled on
    int32_t onItemHitEntity(ServerPlayer &attacker, Actor &target, bool hadEffect, int32_t durabilityDamage);

    // the game keeps one registry: a name stays taken across packs
    bool isCustomComponentRegistered(bool blocks, const std::string &name) const;

    bool isDimensionPending(const std::string &name) const;

    // after startup: ids from DimensionNameIdTable, a level in every world, the table saved back
    void createCustomDimensions();

    void onEntitySpawn(ServerActor &entity);

    void onEntityRemove(ServerActor &entity);

    void onEntityLoad(ServerActor &entity);

    bool beforePlayerInteractWithBlock(ServerPlayer &player, const Vector3i &position, int32_t face);

    bool beforeEntityHurt(Actor &hurtEntity, float damage, const std::string &cause, const Actor *attacker);

    bool beforeEffectAdd(Actor &entity, const MobEffectInstance &effect);

    bool beforePlayerInteractWithEntity(ServerPlayer &player, Actor &target);

    bool beforeItemUse(ServerPlayer &player);

    // rule is the game's lowercase name; packs whose GameRule enum lacks it get nothing
    void onGameRuleChange(const std::string &rule, bool isBool, bool boolValue, int32_t intValue);

    // WeatherType values; scripts may rewrite newWeather and duration (ticks); true when cancelled
    bool beforeWeatherChange(const std::string &previousWeather, std::string &newWeather, int32_t &duration);

    void onWeatherChange(const std::string &dimension, const std::string &previousWeather,
                         const std::string &newWeather);

    // system.afterEvents.scriptEventReceive in every pack, filtered by each subscriber's namespaces
    void emitScriptEvent(const std::string &id, const std::string &message,
                         const ScriptEventOrigin &origin = ScriptEventOrigin());

    // from a running script: delivered on the next tick, never inside the caller's own call
    void queueScriptEvent(const std::string &id, const std::string &message);

    void setWatchdogMs(int64_t milliseconds) {
        mWatchdogMs = milliseconds;
    }

    JSRuntime *runtime() {
        return mRuntime;
    }

    WorldClockSystem &worldClocks() {
        return mClocks;
    }

    // InitializeRegistry for a player who just spawned
    void sendWorldClocks(ServerPlayer &player);

    // under WorldClocks in the default world, the tag itself as the game stores it
    void saveWorldClocks();

private:
    bool _loadPack(const BehaviorPack &pack);

    void _installConsole(JSContext *context);

    void _pumpJobs();

    void _arm();

    // calls fill(api, event) for each pack with subscribers; true when one cancelled
    template <typename Fill>
    bool _emit(const char *key, const char *signal, bool cancellable, Fill fill);

    void _scheduleBlockTick(Level &level, const std::string &blockId, const Vector3i &position);

    // fill(api, event) builds the payload, read(api, event) reads it back once every handler ran
    template <typename Fill, typename Read>
    void _fireComponent(bool blocks, const std::vector<CustomComponentUse> &uses, const char *hook,
                        const char *eventClass, Fill fill, Read read);

    template <typename Fill>
    void _fireComponent(bool blocks, const std::vector<CustomComponentUse> &uses, const char *hook,
                        const char *eventClass, Fill fill);

    friend int _scriptInterruptHandler(JSRuntime *runtime, void *opaque);

    JSRuntime *mRuntime;
    ServerNetworkHandler *mHost = nullptr;
    uint32_t mNativeClassId = 0;
    PlayerHandleRegistry mPlayers;
    std::vector<std::unique_ptr<ScriptApi>> mApis;
    std::mt19937 mTickRandom{std::random_device{}()};
    int64_t mWatchdogMs;
    std::vector<std::pair<std::string, std::string>> mQueuedScriptEvents;
    void _bindClockListener();

    void _broadcastClocks(const SyncWorldClocksPacket &packet);

    WorldClockSystem mClocks;
    int64_t mLastClockSync = 0;
    // last power each redstone consumer custom block received, per level
    std::unordered_map<const Level *, std::unordered_map<int64_t, int32_t>> mRedstonePower;
    int64_t mExecutionDeadlineMs;
};
