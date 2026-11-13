#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "World/Actor/MobEffect.h"
#include "World/Level/Dimension/Dimension.h"
#include "Protocol/Types/ItemStack.h"
#include "Protocol/Types/LocatorBarWaypoint.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/GameTestRunner.h"
#include "World/Level/Block/BlockState.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/Binding/PlayerHandleRegistry.h"
#include "Scripting/Binding/ScriptNatives.h"
#include "Scripting/Binding/ScriptSurface.h"
#include "Scripting/ScriptModules.h"
#include "Scripting/ScriptPackConfiguration.h"
#include "Scripting/ScriptScheduler.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <quickjs.h>

class Actor;
class ActorDamageSource;
class ItemActor;
class Level;
class ServerNetworkHandler;
class ServerPlayer;
class ServerActor;
class CommandOrigin;
class ScriptCustomCommand;

// what the running script may touch: early execution before startup ends, restricted inside before events
enum class ScriptPrivilege { Normal, Early, Restricted };

// WorldSoundOptions and PlayerSoundOptions as read
struct ScriptSoundOptions {
    double mVolume = 1.0;
    double mPitch = 1.0;
    int32_t mLoopCount = 0;
    bool mBroadcast = false;
};

// one behavior pack's scripts: its own context, module versions and official surface
class ScriptNetService;

class ScriptApi {
public:
    static constexpr const char *SCRIPT_EVENT_KEY = "system.after.scriptEventReceive";

    // players share handles across packs: projectile owners store them
    ScriptApi(JSContext *context, ServerNetworkHandler &host, PlayerHandleRegistry &players, JSClassID nativeClassId,
              std::string packName, std::string packRoot, ScriptModuleSet modules);

    ~ScriptApi();

    ScriptApi(const ScriptApi &) = delete;

    ScriptApi &operator=(const ScriptApi &) = delete;

    void install();

    void tick(int64_t currentTick);

    static ScriptApi *fromContext(JSContext *ctx);

    JSContext *context() {
        return mContext;
    }

    ServerNetworkHandler &host() {
        return mHost;
    }

    ScriptScheduler &scheduler() {
        return mScheduler;
    }

    // runs once per tick until it returns false
    void addTickJob(std::function<bool()> job) {
        mTickJobs.push_back(std::move(job));
    }

    const std::string &packName() const {
        return mPackName;
    }

    const std::string &packRoot() const {
        return mPackRoot;
    }

    const ScriptSurface &uiSurface() const {
        return mUiSurface;
    }

    void setPackUuid(std::string packUuid) {
        mPackUuid = std::move(packUuid);
    }

    const std::string &packUuid() const {
        return mPackUuid;
    }

    const ScriptHandleCounts &handleCounts() const {
        return *mHandleCounts;
    }

    // disableWatchdogTimingWarnings: the pack asked for no slow or spike warnings
    bool timingWarningsDisabled() const {
        return mTimingWarningsDisabled;
    }

    void setTimingWarningsDisabled(bool disabled) {
        mTimingWarningsDisabled = disabled;
    }

    // the runner's side of @minecraft/server-gametest; nothing without the module
    std::vector<GameTestRunner::Descriptor> gameTestList();

    std::vector<std::string> gameTestTags();

    // structureBlock: where relative (0, 1, 0) starts; false for a name the pack does not know
    bool gameTestStart(int32_t id, const std::string &name, const Vector3i &structureBlock, int32_t rotation,
                       const Vector3i &size, const Level &level);

    void gameTestStop(int32_t id);

    // setBeforeBatchCallback and setAfterBatchCallback
    void gameTestBatch(bool before, const std::string &batchName);

    const ScriptModuleSet &modules() const {
        return mModules;
    }

    const ScriptSurface &surface() const {
        return mSurface;
    }

    bool isStable1() const {
        return mTrack == ScriptApiTrack::Stable1;
    }

    ScriptPrivilege privilege() const {
        return mPrivilege;
    }

    void setPrivilege(ScriptPrivilege privilege) {
        mPrivilege = privilege;
    }

    template <typename T>
    T *native(JSValueConst value) const {
        return dynamic_cast<T *>((ScriptNative *) JS_GetOpaque(value, mNativeClassId));
    }

    // an instance of an official class; owns data
    JSValue makeInstance(const std::string &className, ScriptNative *data);

    // the params JSON of the custom component the block binds where it stands
    std::optional<std::string> customBlockComponent(Level &level, const Vector3i &position, const std::string &id);

    // the custom component an item type's JSON binds, null without one
    const CustomComponentUse *customItemComponent(const std::string &typeId, const std::string &id) const;

    // a CustomComponentParameters holding the parsed params JSON, undefined params when empty
    JSValue makeComponentParameters(const std::string &paramsJson);

    // queue target.runCommand(line) for the next tick; the promise settles with its result
    JSValue queueCommand(JSValueConst target, JSValueConst line);

    // an optional MolangVariableMap argument: false when value is something else
    bool readMolang(JSValueConst value, std::string &json, bool &present);

    // an event object of the class the signal fires
    JSValue makeEvent(const std::string &signalKey);

    // fields: extra properties of the error object, taken
    JSValue throwError(const std::string &errorClass, const std::string &message, JSValue fields = JS_UNDEFINED);

    JSValue throwInvalidEntity(const char *action, const std::string &member, const ScriptEntityRef &entity);

    // typeId: the stack's type when the component was taken
    JSValue throwInvalidItemStack(const char *action, const std::string &member, const std::string &typeId);

    JSValue makePlayer(ServerPlayer &player);

    // the gametest SimulatedPlayer prototype: a simulated player's handle is made from it
    void setSimulatedPlayerPrototype(JSValueConst prototype) {
        JS_FreeValue(mContext, mSimulatedPlayerPrototype);
        mSimulatedPlayerPrototype = JS_DupValue(mContext, prototype);
    }

    JSValue makeChatTargets(const std::vector<ServerPlayer *> &players);

    JSValue makeActor(ServerActor &actor);

    JSValue makeEntity(Actor &actor);

    // undefined when the entity lacks the component or the pack's version lacks its class
    JSValue makeEntityComponent(Actor &actor, const ScriptEntityRef &ref, const std::string &componentId);

    std::vector<std::string> entityComponentIds(Actor &actor);

    // inventory, equippable, ender_inventory and cursor_inventory: undefined when the entity lacks it
    JSValue makeContainerComponent(Actor &actor, const ScriptEntityRef &ref, const std::string &componentId);

    static std::vector<std::string> containerComponentIds();

    // the Container of a block actor's inventory: chest, barrel, furnace, hopper
    JSValue makeBlockContainer(Level &level, const Vector3i &position);

    // minecraft:type_family of the active definition
    static std::vector<std::string> entityFamilies(Actor &actor);

    ServerPlayer *resolvePlayer(JSValueConst value);

    ServerPlayer *resolvePlayerByHandle(uint32_t handle);

    ServerActor *resolveActor(JSValueConst value);

    Actor *resolveEntity(JSValueConst value);

    Actor *resolveEntity(const ScriptEntityRef &ref);

    JSValue makeItem(const ItemStack &item);

    // the script view of a game stack; dynamic properties come out of the NBT
    static void readItemStack(const ItemStack &item, ScriptItemData &data);

    // the effect's name as scripts see it, without namespace
    static std::string effectTypeId(MobEffectId id);

    JSValue makeEffect(Actor &actor, MobEffectId id);

    // an EntityDamageSource: cause, damagingEntity and damagingProjectile
    JSValue makeDamageSource(const ActorDamageSource &source);

    // the EntityDamageCause of a source
    static std::string damageCauseName(const ActorDamageSource &source);

    JSValue makeHealSource(const char *cause);

    // getModifiers of a data driven trigger: one DefinitionModifier
    JSValue makeModifiersGetter(const std::vector<std::string> &added, const std::vector<std::string> &removed);

    // a wire game type as this pack's GameMode value
    const char *gameModeName(int32_t gameType) const;

    // -1 for a name this pack's GameMode lacks
    int32_t parseGameMode(const std::string &name) const;

    JSValue makeItemStack(const std::string &typeId, int32_t amount);

    JSValue makeHeldItemStack(ServerPlayer &player, const std::string &identifier);

    JSValue makeBlock(const Level &level, int32_t x, int32_t y, int32_t z);

    // default states: the events only carry the block name
    JSValue makePermutation(const std::string &identifier);

    JSValue makePermutation(const BlockState &state);

    // a BlockPermutation's type and states; false when the value is not one
    bool readPermutation(JSValueConst permutation, BlockState &state);

    // the type's default states, from the palette or the pack block
    static BlockState defaultBlockState(const std::string &identifier);

    // sets and broadcasts the block, then the custom block onPlace or onBlockStateChange hooks
    bool setBlock(Level &level, int32_t x, int32_t y, int32_t z, const BlockState &state);

    JSValue makeDimension(const Level &level);

    // undefined when the block lacks the component or the pack's version lacks its class
    JSValue makeBlockComponent(Level &level, const Vector3i &position, const std::string &componentId);

    // a Structure object of the shared registry
    JSValue makeStructure(const std::string &identifier);

    // the locations of a BlockVolumeBase, read through its iterator; false with a pending exception
    bool readVolume(JSValueConst volume, std::vector<Vector3i> &locations);

    // a ListBlockVolume of the given locations
    JSValue makeListVolume(const std::vector<Vector3i> &locations);

    Level &levelOf(JSValueConst dimension);

    // falls back to the default world once unloaded
    Level &resolveLevel(uint32_t worldId, DimensionType dimension);

    bool giveItem(ServerPlayer &player, const std::string &typeId, int32_t amount);

    // false for an unknown item type
    bool toItemStack(const ScriptItemData &item, ItemStack &stack);

    bool setBlockType(Level &level, int32_t x, int32_t y, int32_t z, const std::string &typeId);

    // a CommandResult, or throws CommandError for a command that does not parse; entity null: the dimension runs it
    JSValue runCommand(Level &level, Actor *entity, const std::string &commandLine);

    // CustomCommandRegistry.registerCommand and registerEnum: throw the official errors when refused
    JSValue registerCustomCommand(JSValueConst definition, JSValueConst callback);

    JSValue registerCommandEnum(const std::string &name, std::vector<std::string> values);

    const std::vector<std::string> *findCommandEnum(const std::string &name) const;

    // parses the arguments as the command's parameters and calls its callback; false when it failed
    bool runCustomCommand(const ScriptCustomCommand &command, CommandOrigin &sender,
                          const std::vector<std::string> &arguments);

    void unregisterCustomCommands();

    // logs and clears the context's pending exception
    void logPendingException(const std::string &where);

    // resolves { status, value }: status is Answered, UserClosed, UserBusy, PlayerQuit or ServerShutdown
    JSValue showForm(JSValueConst playerValue, const std::string &formData);

    void closeAllForms(JSValueConst playerValue);

    // value is the store property: numbers become doubles, bigints longs; the form id, or 0 when the player is gone
    JSValue showScreen(JSValueConst playerValue, const std::string &screenId, const std::string &propertyPrefix,
                       JSValueConst value, JSValueConst writable, JSValueConst onWrite, JSValueConst onClose);

    JSValue updateScreen(JSValueConst playerValue, int32_t formId, JSValueConst value, JSValueConst writable);

    void closeScreen(JSValueConst playerValue, int32_t formId);

    // ObservableString.getFilteredText: undefined when the player is gone, else a promise for the text or its errors
    JSValue filterText(JSValueConst playerValue, const std::string &text);

    void addSubscriber(const std::string &key, JSValue callback);

    void removeSubscriber(const std::string &key, JSValueConst callback);

    bool hasSubscribers(const std::string &key) const;

    // the namespaces option of scriptEventReceive.subscribe; the callback must already be subscribed
    void filterScriptEvents(JSValueConst callback, std::vector<std::string> namespaces);

    // clock and marker: the subscribe options a callback must match, empty for any
    void filterClockEvents(JSValueConst callback, std::string clock, std::string marker);

    // emit for a world clock signal: callbacks whose options name another clock or marker are skipped
    bool emitClockEvent(const std::string &key, JSValue eventObject, const std::string &clock,
                        const std::string &marker);

    // the WorldClock object of a registered clock, the same one each time
    JSValue makeClock(const std::string &name);

    JSValue makeTimeMarker(const std::string &clock, const std::string &marker);

    // before events run restricted, startup early; true when a subscriber set cancel
    bool emit(const std::string &key, JSValue eventObject);

    // signals whose subscribe options decide, per callback, which events it gets
    static bool keepsSubscribeOptions(const std::string &key);

    void keepSubscribeOptions(JSValueConst callback, JSValueConst options);

    // Entity.matches against an EntityFilter; a missing filter matches
    static bool entityMatches(JSContext *ctx, JSValueConst entity, JSValueConst filter);

    // emit skipping the callbacks whose kept options accepts refuses
    bool emitFiltered(const std::string &key, JSValue eventObject,
                      const std::function<bool(JSValueConst options)> &accepts);

    // throws the official error and returns JS_EXCEPTION when the registration is refused
    JSValue registerCustomComponent(bool blocks, const std::string &name, JSValueConst handlers);

    bool hasCustomComponent(bool blocks, const std::string &name) const;

    // names registerCustomDimension accepted during startup, created once it ends
    const std::vector<std::string> &pendingDimensions() const {
        return mPendingDimensions;
    }

    JSValue registerCustomDimension(const std::string &name);

    // the startup event's registries accept entries only while it runs
    bool registrationOpen() const {
        return mRegistrationOpen;
    }

    // false when no component this pack registered for the uses defines hook
    bool handlesHook(bool blocks, const std::vector<CustomComponentUse> &uses, const char *hook) const;

    // an instance of a hook payload class; undefined when the pack's module version lacks it
    JSValue makeComponentEvent(const char *eventClass);

    // calls hook(event, params) on each registered component of uses; 1.x hooks take the event alone
    void fireComponents(bool blocks, const std::vector<CustomComponentUse> &uses, const char *hook, JSValueConst event);

    void fillBlockEvent(JSValueConst event, const Level &level, const Vector3i &position);

    void startup();

    // after every pack's startup and the custom dimensions it registered
    void worldLoad();

    void shutdown();


    // EntityQueryOptions over spawned players and, unless playersOnly, live actors; null level: every dimension
    JSValue queryEntities(JSValueConst options, const Level *level, bool playersOnly);

    void setPackSettings(std::vector<PackSetting> settings) {
        mPackSettings = std::move(settings);
    }

    const std::vector<PackSetting> &packSettings() const {
        return mPackSettings;
    }

    // the manifest's script module uuid: names its config/ folder
    void setScriptModuleUuid(std::string uuid) {
        mScriptModuleUuid = std::move(uuid);
    }

    const std::string &scriptModuleUuid() const {
        return mScriptModuleUuid;
    }

    void setScriptModuleVersion(std::string version) {
        mScriptModuleVersion = std::move(version);
    }

    const std::string &scriptModuleVersion() const {
        return mScriptModuleVersion;
    }

    const ScriptSurface &diagnosticsSurface() const {
        return mDiagnosticsSurface;
    }

    // server-admin's SecretString table, undefined without that module
    JSValue secretOf() const {
        return mSecretOf;
    }

    // requests no script waits on: their ids stay clear of the ones server-net hands out
    uint32_t nextUntrackedRequestId() {
        return mUntrackedRequestId++;
    }

    // sentry's dsn and envelope url: kept here, a secret dsn never reaches the pack's code
    std::string &diagnosticsDsn() {
        return mDiagnosticsDsn;
    }

    std::string &diagnosticsUrl() {
        return mDiagnosticsUrl;
    }

    const ScriptSurface &gameTestSurface() const {
        return mGameTestSurface;
    }

    const ScriptSurface &adminSurface() const {
        return mAdminSurface;
    }

    const ScriptSurface &netSurface() const {
        return mNetSurface;
    }

    // module_permissions of @minecraft/server-net, read when the module was installed
    const ScriptNetModuleConfig &netConfig() const {
        return mNetConfig;
    }

    ScriptNetService &netService();

    // { httpDone, httpFail, wsOpened, ... } of the net bootstrap
    JSValue netHooks() const {
        return mNetHooks;
    }

    void setPacketInterest(bool receive, bool interested) {
        (receive ? mWantsPacketReceive : mWantsPacketSend) = interested;
    }

    bool wantsPacketReceive() const {
        return mWantsPacketReceive;
    }

    bool wantsPacketSend() const {
        return mWantsPacketSend;
    }

    // beforeEvents.packetReceive and packetSend of @minecraft/server-net; true when cancelled
    bool emitPacketReceive(const char *packetId, size_t size, ServerPlayer *sender);

    bool emitPacketSend(const char *packetId, const std::vector<ServerPlayer *> &recipients);

    // an object of no official class holding data for the natives
    JSValue makeOpaque(ScriptNative *data);

    // { classes, make } of the error bootstrap
    JSValue errors() const {
        return mErrors;
    }

    // asyncPlayerJoin of @minecraft/server-admin; false when the pack lacks the module or a subscriber
    bool emitPlayerJoin(uint32_t token, const std::string &name, const std::string &persistentId);

    bool hasJoinSubscribers();

    void forgetEntity(uint64_t runtimeId);

    void forgetPlayer(ServerPlayer &player);

    // a waypoint in one player's locator bar and what the client last received for it
    struct LocatorEntry {
        std::shared_ptr<ScriptWaypointState> mState;
        JSValue mObject = JS_UNDEFINED;
        Uuid mHandle;
        bool mSent = false;
        LocatorBarWaypoint mSentWaypoint;
    };

    std::vector<LocatorEntry> &locatorBarOf(uint32_t handle) {
        return mLocatorBars[handle];
    }

    Uuid nextWaypointHandle();

    // a server sound handle unique across packs
    static int64_t nextSoundHandle();

    // WorldSoundOptions, or PlayerSoundOptions without isBroadcast; an exception value when refused, undefined otherwise
    JSValue readSoundOptions(JSValueConst options, ScriptSoundOptions &read, bool world);

    // the SoundInstance; recipient: Player.playSound's player, null for every player in the level
    JSValue playSound(Level &level, ServerPlayer *recipient, const std::string &eventId, const Vector3f &position,
                      const ScriptSoundOptions &options);

    JSValue soundDefinitionRegistry() const {
        return mSoundDefinitionRegistry;
    }

    JSValue lootTableManager() const {
        return mLootTableManager;
    }

    JSValue primitiveShapesManager() const {
        return mPrimitiveShapes;
    }

    void emitProjectileHitBlock(ServerActor &projectile, int32_t x, int32_t y, int32_t z, const Vector3f &from,
                                const Vector3f &to);

    void emitItemUseOnBlock(ServerPlayer &player, int32_t x, int32_t y, int32_t z);

    // the custom block the actor now stands on, or an empty id: fires onStepOff then onStepOn on a change
    void emitBlockStep(Actor &actor, const Level &level, const CustomBlockDefinition *block, const Vector3i &position);

    bool hasSteppingActors() const {
        return !mStepping.empty();
    }

    // EntityQueryOptions.scoreOptions: every option must hold
    bool scoresMatch(const Actor &actor, JSValueConst options);

    JSValue moduleScoreboard() const {
        return mScoreboard;
    }

    JSValue moduleTickingAreas() const {
        return mTickingAreas;
    }

    // the ticking areas this pack's manager added, by identifier
    std::vector<std::string> &tickingAreaNames() {
        return mTickingAreaNames;
    }

    struct PendingTickingArea {
        std::string mName;
        JSValue mResolve;
        JSValue mReject;
    };

    std::vector<PendingTickingArea> &pendingTickingAreas() {
        return mPendingTickingAreas;
    }

    void resolveTickingAreas();

    JSValue structureManager() const {
        return mStructureManager;
    }

    JSValue gameRules() const {
        return mGameRules;
    }

    JSValue serverSystemInfo() const {
        return mSystemInfo;
    }

    JSValue worldAfterEvents() const {
        return mAfterEvents;
    }

    JSValue worldBeforeEvents() const {
        return mBeforeEvents;
    }

    JSValue systemAfterEvents() const {
        return mSystemAfterEvents;
    }

    JSValue systemBeforeEvents() const {
        return mSystemBeforeEvents;
    }

    struct Binding {
        enum class Kind { Function, Getter, Setter, Constructor };

        std::string mName;
        Kind mKind = Kind::Function;
        JSCFunction *mFunction = nullptr;
        JSCFunctionMagic *mMagicFunction = nullptr;
        int mMagic = 0;
        bool mEarly = false;
        bool mBlocked = false;
        // Entity and Player members check the entity still exists first
        bool mEntity = false;
        // on the constructor: no bound object
        bool mStatic = false;
        // the opaque type the native reads: a script can graft any object onto any prototype
        bool (*mAccepts)(const ScriptNative *) = nullptr;
    };

    const Binding &binding(int index) const {
        return mBindings[(size_t) index];
    }

    // false and nothing defined when the pack's module version does not declare cls.name
    bool method(JSValueConst target, const char *cls, const char *name, JSCFunction *function, int length);

    // every signal has subscribe and unsubscribe, even where the definitions put them on a base interface
    void signalMethod(JSValueConst target, const char *cls, const char *name, JSCFunction *function);

    bool methodMagic(JSValueConst target, const char *cls, const char *name, JSCFunctionMagic *function, int length,
                     int magic);

    bool property(JSValueConst target, const char *cls, const char *name, JSCFunction *getter,
                  JSCFunction *setter = nullptr);

    bool propertyMagic(JSValueConst target, const char *cls, const char *name, JSCFunctionMagic *getter, int magic,
                       JSCFunctionMagic *setter = nullptr);

    // constructor and prototype of an official class; constructors throw unless a native constructor is given
    JSValue defineClass(const std::string &name, JSCFunction *constructor = nullptr, int length = 0);

    JSValue prototypeOf(const std::string &className) const;

    const std::vector<std::pair<std::string, JSValue>> &moduleExports(const std::string &module) const;

    JSValue constructorOf(const std::string &className) const;

private:
    struct ScriptClass {
        JSValue mConstructor = JS_UNDEFINED;
        JSValue mPrototype = JS_UNDEFINED;
    };

    int _addBinding(Binding binding);

    void _bindWorld();

    void _bindTickingAreas();

    void _bindSystem();

    void _bindEntity();

    void _bindPlayer();

    void _bindComponents();

    void _bindEntityComponents();

    void _bindBlock();

    void _bindBlockSurface();

    void _bindDimensionSurface();

    void _bindPoi();

    void _bindStructures();

    void _bindItemStack();

    void _bindContainers();

    void _bindDimension();

    void _bindEvents();

    void _bindRegistries();

    void _installBootstraps();

    void _defineEnums();

    void _registerModules();

    void _subscribeGameEvents();

    void _emitInventoryChanges();

    void _emitCursorChanges();

    void _resolveForm(uint32_t formId, const std::string &response, const char *status);

    void _resolveFilteredTexts();

    void _fireStep(Actor &actor, const Level &level, const std::string &blockId, const Vector3i &position,
                   const char *hook, const char *eventClass);

    JSValue _makeSignalHolder(const char *className, const char *signalOwner, const char *keyPrefix);

    bool _acceptsScriptEvent(JSValueConst callback, const std::string &eventNamespace) const;

    bool _acceptsClockEvent(JSValueConst callback) const;

    // signals whose kept options the event object itself answers: entity, block, input, hotbar and swing filters
    static bool _readsEventOptions(const std::string &key);

    bool _acceptsEventOptions(const std::string &key, JSValueConst options, JSValueConst event);

    void _bindClocks();

    void _bindScoreboard();

    void _bindCommands();

    void _bindWaypoints();

    void _tickLocatorBars();

    void _bindSounds();

    // a 1.x runCommandAsync waiting for the next tick
    struct QueuedCommand {
        JSValue mTarget = JS_UNDEFINED;
        JSValue mLine = JS_UNDEFINED;
        JSValue mResolvers[2] = {JS_UNDEFINED, JS_UNDEFINED};
    };

    void _bindMolang();

    void _bindFog();

    void _bindAimAssist();

    void _runQueuedCommands();

    void _freeQueuedCommand(QueuedCommand &command);

    void _tickSounds();

    // the natives the loot bootstrap reads its tables and rolls through; takes itemTypes
    JSValue _lootNatives(JSValue itemTypes);

    void _bindLootTableManager();

    // @minecraft/debug-utilities: the drawer, the diagnostics manager and the stats functions
    void _installDebugUtilities();

    // @minecraft/diagnostics: the sentry client
    void _installDiagnostics();

    // @minecraft/server-gametest: registrations, the Test helper and the hooks the runner drives
    void _installGameTest();

    JSContext *mContext;
    ServerNetworkHandler &mHost;
    JSClassID mNativeClassId;
    std::string mPackName;
    std::string mPackRoot;
    std::string mPackUuid;
    ScriptModuleSet mModules;
    ScriptApiTrack mTrack;
    ScriptSurface mSurface;
    ScriptSurface mUiSurface;
    ScriptSurface mCommonSurface;
    ScriptSurface mGraphicsSurface;
    ScriptSurface mAdminSurface;
    ScriptSurface mNetSurface;
    ScriptSurface mDebugSurface;
    ScriptSurface mDiagnosticsSurface;
    ScriptSurface mGameTestSurface;
    ScriptPrivilege mPrivilege = ScriptPrivilege::Normal;

    struct SteppedBlock {
        std::string mIdentifier;
        Vector3i mPosition;
    };

    // the custom block each actor stands on: fires once per step, not per tick
    std::unordered_map<int64_t, SteppedBlock> mStepping;
    ScriptScheduler mScheduler;
    std::vector<std::function<bool()>> mTickJobs;

    std::vector<QueuedCommand> mQueuedCommands;
    PlayerHandleRegistry &mPlayers;

    // a deque: a native may define a class, and so a binding, while its own binding is in use
    std::deque<Binding> mBindings;
    std::unordered_map<std::string, ScriptClass> mClasses;
    std::vector<std::pair<std::string, JSValue>> mServerExports;
    std::vector<std::pair<std::string, JSValue>> mUiExports;
    std::vector<std::pair<std::string, JSValue>> mCommonExports;
    std::vector<std::pair<std::string, JSValue>> mGraphicsExports;
    std::vector<std::pair<std::string, JSValue>> mAdminExports;
    std::vector<std::pair<std::string, JSValue>> mNetExports;
    // what the admin bootstrap hands back to the engine: emitJoin
    JSValue mAdminHooks = JS_UNDEFINED;
    JSValue mNetHooks = JS_UNDEFINED;
    ScriptNetModuleConfig mNetConfig;
    std::unique_ptr<ScriptNetService> mNetService;
    bool mWantsPacketReceive = false;
    bool mWantsPacketSend = false;
    std::string mScriptModuleUuid;
    std::string mScriptModuleVersion;
    JSValue mSecretOf = JS_UNDEFINED;
    uint32_t mUntrackedRequestId = 0x80000000u;
    std::string mDiagnosticsDsn;
    std::string mDiagnosticsUrl;
    std::vector<std::pair<std::string, JSValue>> mDebugExports;
    std::vector<std::pair<std::string, JSValue>> mDiagnosticsExports;
    std::vector<std::pair<std::string, JSValue>> mGameTestExports;
    JSValue mGameTestHooks = JS_UNDEFINED;
    JSValue mSimulatedPlayerPrototype = JS_UNDEFINED;
    std::shared_ptr<ScriptHandleCounts> mHandleCounts = std::make_shared<ScriptHandleCounts>();
    bool mTimingWarningsDisabled = false;
    JSValue mDebugTick = JS_UNDEFINED;

    // same entity, same object: scripts compare and key maps by them
    std::unordered_map<int64_t, JSValue> mActorObjects;
    std::unordered_map<uint32_t, JSValue> mPlayerObjects;
    std::unordered_map<uint64_t, JSValue> mDimensionObjects;
    std::unordered_map<uint32_t, std::vector<LocatorEntry>> mLocatorBars;
    uint64_t mNextWaypointHandle = 0;

    JSValue mWorld = JS_UNDEFINED;
    JSValue mSystem = JS_UNDEFINED;
    JSValue mAfterEvents = JS_UNDEFINED;
    JSValue mBeforeEvents = JS_UNDEFINED;
    JSValue mSystemAfterEvents = JS_UNDEFINED;
    JSValue mSystemBeforeEvents = JS_UNDEFINED;
    JSValue mScoreboard = JS_UNDEFINED;
    JSValue mTickingAreas = JS_UNDEFINED;
    std::vector<std::string> mTickingAreaNames;
    std::vector<PendingTickingArea> mPendingTickingAreas;
    struct PendingFilteredText {
        JSValue mResolve;
        JSValue mValue;
        std::chrono::steady_clock::time_point mReadyAt;
    };
    std::vector<PendingFilteredText> mPendingFilteredTexts;
    JSValue mGameRules = JS_UNDEFINED;
    JSValue mSystemInfo = JS_UNDEFINED;
    std::vector<PackSetting> mPackSettings;
    JSValue mErrors = JS_UNDEFINED;
    JSValue mBlockRegistry = JS_UNDEFINED;
    JSValue mItemRegistry = JS_UNDEFINED;
    JSValue mStructureManager = JS_UNDEFINED;
    JSValue mListVolume = JS_UNDEFINED;
    JSValue mLootTableManager = JS_UNDEFINED;
    JSValue mSoundDefinitionRegistry = JS_UNDEFINED;
    JSValue mPrimitiveShapes = JS_UNDEFINED;
    std::vector<std::shared_ptr<ScriptSoundState>> mTrackedSounds;

    std::unordered_map<std::string, std::vector<JSValue>> mSubscribers;
    std::unordered_map<void *, std::vector<std::string>> mScriptEventNamespaces;
    std::unordered_map<void *, std::pair<std::string, std::string>> mClockFilters;
    std::unordered_map<void *, JSValue> mSubscribeOptions;
    const std::function<bool(JSValueConst)> *mEmitAccepts = nullptr;
    std::string mEmitClock;
    std::string mEmitMarker;
    std::unordered_map<std::string, JSValue> mClockObjects;
    JSValue mClockRegistry = JS_UNDEFINED;
    JSValue mDimensionRegistry = JS_UNDEFINED;
    std::vector<std::string> mPendingDimensions;
    JSValue mCommandRegistry = JS_UNDEFINED;
    // one namespace per pack for its commands and enums
    std::string mCommandNamespace;
    std::unordered_map<std::string, std::vector<std::string>> mCommandEnums;
    std::vector<std::string> mCustomCommands;
    std::unordered_map<std::string, JSValue> mBlockComponents;
    std::unordered_map<std::string, JSValue> mItemComponents;
    bool mRegistrationOpen = false;
    std::unordered_map<uint64_t, std::vector<ItemStack>> mInventorySnapshots;
    std::unordered_map<uint64_t, ItemStack> mCursorSnapshots;

    uint32_t mNextFormId = 1;
    std::unordered_map<uint32_t, JSValue> mFormResolvers;
};

// natives of the data bootstrap, from ScriptBlockBinding
namespace ScriptBlockData {
    JSCFunction *blockInfo();

    JSCFunction *permutationItem();
}

namespace ScriptItemTypeData {
    // the localization key of an item type id, as a fresh stack of it has
    JSCFunction *localizationKey();
}
