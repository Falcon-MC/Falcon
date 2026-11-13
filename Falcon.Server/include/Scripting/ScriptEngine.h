#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "Network/NetworkIdentifier.h"
#include "Protocol/MinecraftPacketIds.h"
#include "Scripting/AimAssistRegistry.h"
#include "Scripting/Binding/PlayerHandleRegistry.h"
#include "Scripting/GameTestRunner.h"
#include "Scripting/ScriptPackConfiguration.h"
#include "Scripting/ScriptSoundRegistry.h"
#include "Scripting/ScriptStructureRegistry.h"
#include "Scripting/WorldClockSystem.h"
#include "World/Level/NeighborChangeQueue.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

struct JSRuntime;
struct JSContext;
struct BehaviorPack;
struct MobEffectInstance;
struct CustomComponentUse;
class BlockState;
class Actor;
class ActorDamageSource;
class ItemActor;
class ItemStack;
class Level;
class MobActor;
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


    // from: where the projectile was, to: where it ended inside the block
    void onProjectileHitBlock(ServerActor &projectile, int32_t x, int32_t y, int32_t z, const Vector3f &from,
                              const Vector3f &to);

    // clickPosition: the face location, relative to the block's bottom north-west corner
    void onItemUseOnBlock(ServerPlayer &player, const Vector3i &position, int32_t face, const Vector3f &clickPosition);

    void onProjectileHitEntity(ServerActor &projectile, Actor &hitEntity, const Vector3f &location);

    void onEntityHitBlock(Actor &damagingEntity, const Vector3i &position, int32_t face);

    void onEntityHitEntity(Actor &damagingEntity, Actor &hitEntity);

    void onEntityItemDrop(Actor &entity, ItemActor *dropped);

    void onPlayerInteractWithBlock(ServerPlayer &player, const std::string &blockId, const Vector3i &position,
                                   int32_t face, const Vector3f &clickPosition);

    // the custom block's onPlayerInteract, without the world's playerInteractWithBlock event
    void onCustomBlockUse(ServerPlayer &player, const std::string &blockId, const Vector3i &position, int32_t face,
                          const Vector3f &clickPosition);

    void onCustomBlockTick(Level &level, const std::string &blockId, const Vector3i &position);

    void onCustomBlockRandomTick(Level &level, const std::string &blockId, const Vector3i &position);

    // a named tick fell due at a block still of the type it was scheduled on
    void onCustomBlockNamedTick(Level &level, const Vector3i &position, const BlockState &state,
                                const std::string &eventNamespace, const std::string &eventName);

    // the tick's neighbor changes of a block with minecraft:neighbor_change, still of the type they were seen on
    void onCustomBlockNeighborChanged(Level &level, const Vector3i &position, const BlockState &state,
                                      const std::vector<NeighborChangeQueue::Change> &changes);

    void onCustomBlockStep(Actor &actor, const Level &level, const std::string &blockId, const Vector3i &position);

    void onCustomBlockStepOff(Actor &actor, const Level &level);

    void onCustomBlockFallOn(Actor &actor, Level &level, const std::string &blockId, const Vector3i &position,
                             float fallDistance);

    // the power the block receives; hooks fire on a change reaching minecraft:redstone_consumer's min_power
    // a level about to be freed: another one may later get its address
    void forgetLevel(const Level &level);

    void onCustomBlockRedstone(Level &level, const std::string &blockId, const Vector3i &position, int32_t power);

    // beforeOnPlayerPlace; true when cancelled, placed takes the permutation scripts chose
    bool beforeCustomBlockPlace(ServerPlayer &player, Level &level, const Vector3i &position, int32_t face,
                                BlockState &placed);

    // world before event of a valid player placement; true when cancelled
    bool beforePlayerPlaceBlock(ServerPlayer &player, Level &level, const Vector3i &position, int32_t face,
                                const Vector3f &faceLocation, const BlockState &placed);

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

    // the used stack, read before the game decrements it; useDuration: ticks left of the charge
    void onItemCompleteUse(ServerPlayer &player, const ItemStack &used, int32_t useDuration = 0);

    void onItemStartUse(ServerPlayer &player, const ItemStack &used, int32_t useDuration);

    // release: the player let go of a charged item; every end of a use also fires itemStopUse
    void onItemStopUse(ServerPlayer &player, const ItemStack &used, int32_t useDuration, bool released);

    // true when a script cancelled the pickup
    bool beforeEntityItemPickup(Actor &collector, Actor &item, const ItemStack &picked);

    void onEntityItemPickup(Actor &collector, const ItemStack &picked);

    // container: the entity whose inventory window opened, source: who opened it
    void onEntityContainerOpened(Actor &container, Actor &source);

    void onEntityContainerClosed(Actor &container, Actor &source);

    // a block's container window, opened or closed by source
    void onBlockContainerOpened(Level &level, const Vector3i &position, Actor &source);

    void onBlockContainerClosed(Level &level, const Vector3i &position, Actor &source);

    void onPlayerStartBreakingBlock(ServerPlayer &player, const Vector3i &position, int32_t face);

    // a break that stopped before the block broke; progress: 0 to 1
    void onPlayerCancelBreakingBlock(ServerPlayer &player, const Vector3i &position, int32_t face, double progress);

    void onItemStartUseOn(ServerPlayer &player, const Vector3i &position, int32_t face);

    void onItemStopUseOn(ServerPlayer &player, const Vector3i &position);

    void onPlayerInputPermissionCategoryChange(ServerPlayer &player, int32_t category, bool enabled);

    // a tracked sound's declared duration elapsed, every loop included
    void onSoundCompleted(const std::string &soundInstanceId);

    // food only, before onCompleteUse: the food component fires it once eaten
    void onItemConsume(ServerPlayer &player, const ItemStack &used);

    // onBeforeDurabilityDamage then onHitEntity; returns the durability damage scripts settled on
    int32_t onItemHitEntity(ServerPlayer &attacker, Actor &target, bool hadEffect, int32_t durabilityDamage);

    // the game keeps one registry: a name stays taken across packs
    bool isCustomComponentRegistered(bool blocks, const std::string &name) const;

    bool isDimensionPending(const std::string &name) const;

    // a custom command enum any pack registered; null when none did
    const std::vector<std::string> *findCommandEnum(const std::string &name) const;

    // a call into scripts from outside: arms the watchdog, then runs the microtasks; nested calls only run
    void runGuarded(const std::function<void()> &call);

    // after startup: ids from DimensionNameIdTable, a level in every world, the table saved back
    void createCustomDimensions();

    // cause: an EntityInitializationCause value
    void onEntitySpawn(ServerActor &entity, const char *cause = "Spawned");

    void beforeEntityRemove(ServerActor &entity);

    void onEntityRemove(ServerActor &entity);

    void onEntityLoad(ServerActor &entity);

    bool beforePlayerInteractWithBlock(ServerPlayer &player, const Vector3i &position, int32_t face);

    // scripts may lower or raise damage; true when cancelled
    bool beforeEntityHurt(Actor &hurtEntity, float &damage, const ActorDamageSource &source);

    void onEntityHurt(Actor &hurtEntity, float damage, const ActorDamageSource &source);

    void onEntityDie(Actor &deadEntity, const ActorDamageSource &source);

    // scripts may rewrite the duration; true when cancelled
    bool beforeEffectAdd(Actor &entity, MobEffectInstance &effect);

    void onEffectAdd(Actor &entity, const MobEffectInstance &effect);

    bool beforePlayerInteractWithEntity(ServerPlayer &player, Actor &target);

    // a melee attack about to land; aim is null when the attack has no direction; true when cancelled
    bool beforePlayerItemAttackEntity(ServerPlayer &player, Actor &target, const Vector3f *aim);

    // after a name tag named the entity; previous is empty when it had no name
    void onPlayerUseNameTag(ServerPlayer &player, Actor &named, const std::string &newName,
                            const std::string &previousName);

    void onPlayerInteractWithEntity(ServerPlayer &player, Actor &target, const ItemStack &before);

    void onEntitySneak(Actor &entity, bool started);

    bool beforeEntityTamed(Actor &entity, Actor &tamer);

    void onEntityTamed(Actor &entity, Actor &tamer);

    void onEntityUpgrade(MobActor &entity, const std::string &upgradeId, const std::vector<std::string> &added,
                         const std::vector<std::string> &removed);

    void onDataDrivenEntityTrigger(MobActor &entity, const std::string &eventId, const std::vector<std::string> &added,
                                   const std::vector<std::string> &removed);

    // game types as on the wire; scripts may pick another mode; true when cancelled
    bool beforePlayerGameModeChange(ServerPlayer &player, int32_t fromGameMode, int32_t &toGameMode);

    void onPlayerGameModeChange(ServerPlayer &player, int32_t fromGameMode, int32_t toGameMode);

    void onPlayerDimensionChange(ServerPlayer &player, const Level &from, const Vector3f &fromLocation, const Level &to,
                                 const Vector3f &toLocation);

    void onPlayerEmote(ServerPlayer &player, const std::string &personaPieceId);

    // InputMode values
    void onPlayerInputModeChange(ServerPlayer &player, const char *previous, const char *current);

    // InputButton and ButtonState values
    void onPlayerButtonInput(ServerPlayer &player, const char *button, bool pressed);

    // EntitySwingSource value
    void onPlayerSwingStart(ServerPlayer &player, const char *source);

    void beforePlayerLeave(ServerPlayer &player);

    bool beforeItemUse(ServerPlayer &player);

    // rule is the game's lowercase name; packs whose GameRule enum lacks it get nothing
    void onGameRuleChange(const std::string &rule, bool isBool, bool boolValue, int32_t intValue);

    // explosion before event: scripts may cancel or rewrite the impacted blocks; true when cancelled
    bool beforeExplosion(Level &level, const ServerActor *source, std::vector<Vector3i> &blocks);

    void onExplosion(Level &level, const ServerActor *source, const std::vector<Vector3i> &blocks);

    // one destroyed block; exploded is the block before the explosion
    void onBlockExplode(Level &level, const Vector3i &position, const BlockState &exploded, const ServerActor *source);

    void onPistonActivate(Level &level, const Vector3i &position, bool expanding);

    // sources: the entities on the wire, none once it releases
    void onTripWireTrip(Level &level, const Vector3i &position, bool powered, const std::vector<Actor *> &sources);

    void onTargetBlockHit(Level &level, const Vector3i &position, const Vector3f &hitVector, int32_t previousPower,
                          int32_t power, Actor *source);

    // source: the entity that pressed it, null for none
    void onButtonPush(Level &level, const Vector3i &position, Actor *source);

    void onLeverAction(Level &level, const Vector3i &position, bool powered, ServerPlayer &player);

    // a plate going from off to on pushes, from on to off pops
    void onPressurePlate(Level &level, const Vector3i &position, int32_t previousPower, int32_t power, Actor *source);

    // WeatherType values; scripts may rewrite newWeather and duration (ticks); true when cancelled
    bool beforeWeatherChange(const std::string &previousWeather, std::string &newWeather, int32_t &duration);

    void onWeatherChange(const std::string &dimension, const std::string &previousWeather,
                         const std::string &newWeather);

    // system.afterEvents.scriptEventReceive in every pack, filtered by each subscriber's namespaces
    void emitScriptEvent(const std::string &id, const std::string &message,
                         const ScriptEventOrigin &origin = ScriptEventOrigin());

    // from a running script: delivered on the next tick, never inside the caller's own call
    void queueScriptEvent(const std::string &id, const std::string &message);

    const std::vector<std::unique_ptr<ScriptApi>> &getApis() const {
        return mApis;
    }

    void setWatchdogMs(int64_t milliseconds) {
        mWatchdogMs = milliseconds;
    }

    JSRuntime *runtime() {
        return mRuntime;
    }

    WorldClockSystem &worldClocks() {
        return mClocks;
    }

    ScriptSoundRegistry &sounds() {
        return mSounds;
    }

    // world.getAimAssist() of every pack
    AimAssistRegistry &aimAssist() {
        return mAimAssist;
    }

    // a script-added entry: every spawned player receives it
    void broadcastAimAssist(const CameraAimAssistPresetsPacket &packet);

    void sendAimAssist(ServerPlayer &player);

    // world.structureManager of every pack
    ScriptStructureRegistry &structures() {
        return mStructures;
    }

    // mobs a block entity storage took this tick: they leave the world with the main thread tasks
    std::unordered_set<int64_t> &storedActors() {
        return mStoredActors;
    }

    // the tests every pack registered with @minecraft/server-gametest
    GameTestRunner &gameTests() {
        return mGameTests;
    }

    // config/ of the server root: the module permissions, variables and secrets of each script module
    ScriptPackConfigurationManager &packConfigs() {
        return mPackConfigs;
    }

    // asyncPlayerJoin: true when a pack holds the join; done runs once every subscriber settled and no script
    // refused the player, with the player still connected
    bool holdPlayerJoin(ServerPlayer &player, std::function<void()> done);

    bool isHoldingJoin(const NetworkIdentifier &id) const;

    // the join event's natives; false once the player left or a script already turned it away
    bool isJoinPending(uint32_t token) const;

    // refuse: disallowJoin or disconnect; a refused join ends now, an allowed one when every promise settled
    bool decideJoin(uint32_t token, bool refuse, const std::string &reason);

    // one pack's promises settled; rejected turns the player away
    void settleJoin(uint32_t token, bool rejected);

    // beforeEvents.packetReceive of @minecraft/server-net; true when a script cancelled it
    bool beforePacketReceive(const NetworkIdentifier &id, MinecraftPacketIds packetId, size_t size);

    // beforeEvents.packetSend; true when a script cancelled it for every recipient
    bool beforePacketSend(const std::vector<NetworkIdentifier> &recipients, MinecraftPacketIds packetId);

    // a pack subscribed or unsubscribed: the send hook is only installed while one listens
    void updatePacketHooks();

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

    // entityContainerOpened and entityContainerClosed: sourceField is openSource or closeSource
    void _emitContainerAccess(const char *key, const char *signal, const char *sourceField, Actor &container,
                              Actor &source);

    void _emitBlockContainerAccess(const char *key, const char *signal, const char *sourceField, Level &level,
                                   const Vector3i &position, Actor &source);

    // progress only for playerCancelBreakingBlock
    void _emitBreakingBlock(const char *key, const char *signal, ServerPlayer &player, const Vector3i &position,
                            int32_t face, const double *progress);

    void _onHealthChanged(Actor &entity, float oldValue, float newValue);

    void _onHeal(Actor &entity, float amount, int32_t cause);

    bool _beforeHeal(Actor &entity, float &amount, int32_t cause);

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
    ScriptStructureRegistry mStructures;
    std::unordered_set<int64_t> mStoredActors;
    GameTestRunner mGameTests;
    ScriptSoundRegistry mSounds;
    AimAssistRegistry mAimAssist;
    int64_t mLastClockSync = 0;
    // last power each redstone consumer custom block received, per level
    std::unordered_map<const Level *, std::unordered_map<int64_t, int32_t>> mRedstonePower;
    struct PendingJoin {
        NetworkIdentifier mId;
        int32_t mOutstanding = 0;
        bool mRefused = false;
        std::function<void()> mDone;
    };

    void _refuseJoin(PendingJoin &join, const std::string &reason);

    ScriptPackConfigurationManager mPackConfigs;
    std::map<uint32_t, PendingJoin> mPendingJoins;
    uint32_t mNextJoinToken = 1;
    int64_t mExecutionDeadlineMs;
    int32_t mGuardDepth = 0;
};
