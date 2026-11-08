#pragma once

#include "Core/Math/Vector3i.h"
#include "World/Level/Dimension/Dimension.h"
#include "Protocol/Types/ItemStack.h"
#include "Scripting/Binding/PlayerHandleRegistry.h"
#include "Scripting/ScriptScheduler.h"

#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include <quickjs.h>

class Actor;
class Level;
class ServerNetworkHandler;
class ServerPlayer;
class ServerActor;

enum class ScriptEvent : int {
    AfterPlayerBreakBlock = 0,
    AfterPlayerPlaceBlock,
    AfterPlayerSpawn,
    AfterPlayerJoin,
    AfterPlayerLeave,
    AfterChatSend,
    AfterEntityHurt,
    AfterEntityDie,
    AfterItemUse,
    BeforePlayerBreakBlock,
    BeforeChatSend,
    Count
};

class ScriptApi {
public:
    ScriptApi(JSContext *context, JSRuntime *runtime, ServerNetworkHandler &host);

    ~ScriptApi();

    ScriptApi(const ScriptApi &) = delete;

    ScriptApi &operator=(const ScriptApi &) = delete;

    void install();

    void tick(int64_t currentTick);

    JSContext *context() { return mContext; }

    ServerNetworkHandler &host() { return mHost; }

    ScriptScheduler &scheduler() { return mScheduler; }

    JSValue makePlayer(ServerPlayer &player);

    ServerPlayer *resolvePlayer(JSValueConst value);

    ServerPlayer *resolvePlayerByHandle(uint32_t handle);

    bool giveItem(ServerPlayer &player, const std::string &typeId, int32_t amount);

    bool setBlockType(Level &level, int32_t x, int32_t y, int32_t z, const std::string &typeId);

    int32_t runCommandAsPlayer(ServerPlayer &player, const std::string &commandLine);

    int32_t runCommandAsConsole(const std::string &commandLine);

    JSValue showForm(JSValueConst playerValue, const std::string &formData);

    JSValue makeActor(ServerActor &actor);

    ServerActor *resolveActor(JSValueConst value);

    JSValue makeEntity(Actor &actor);

    JSValue makeItem(const ItemStack &item);

    JSClassID actorClassId() const { return mActorClassId; }

    JSValue makeBlock(const Level &level, int32_t x, int32_t y, int32_t z);

    JSValue makeItemStack(const std::string &typeId, int32_t amount);

    JSValue makeHeldItemStack(ServerPlayer &player, const std::string &identifier);

    JSValue makeDimension(const Level &level);

    // `id` names the dimension, a hidden world id set by makeDimension the world;
    // objects built by scripts resolve in the default world
    Level &levelOf(JSContext *ctx, JSValueConst dimension);

    // falls back to the default world once unloaded
    Level &resolveLevel(uint32_t worldId, DimensionType dimension);

    JSClassID blockClassId() const { return mBlockClassId; }

    JSClassID itemStackClassId() const { return mItemStackClassId; }

    void addSubscriber(ScriptEvent event, JSValue callback);

    void removeSubscriber(ScriptEvent event, JSValueConst callback);

    void addNamedSubscriber(const std::string &name, JSValue callback);

    void removeNamedSubscriber(const std::string &name, JSValueConst callback);

    bool emitNamed(const std::string &name, JSValue eventObject);

    bool hasNamedSubscribers(const std::string &name) const;

    void emitProjectileHitBlock(ServerActor &projectile, int32_t x, int32_t y, int32_t z);

    void emitItemUseOnBlock(ServerPlayer &player, int32_t x, int32_t y, int32_t z);

    void emitBlockInteract(ServerPlayer &player, const std::string &blockId, const Vector3i &position,
                           const char *face);

    void emitBlockTick(Level &level, const std::string &blockId, const Vector3i &position);

    void emitBlockStep(Actor &actor, const Level &level, const std::string &blockId, const Vector3i &position);

    void emitBlockStepOff(Actor &actor, const Level &level);

    bool hasSteppingActors() const {
        return !mStepping.empty();
    }

    void emitWorldInitialize();

    void emitWorldLoad();

    void fireHeldItemComponents(ServerPlayer &player, const char *hook, JSValue event);

    void fireBlockComponents(const std::string &blockId, const char *hook, JSValue event);

    JSValue _makeBlockEvent(const Level &level, const Vector3i &position);

    void _scheduleBlockTick(Level &level, const std::string &blockId, const Vector3i &position);

    void _fireComponents(const char *dispatcher, const std::vector<std::string> *components, const char *hook,
                         JSValue event);

    JSValue moduleWorld() const { return mWorld; }

    JSValue moduleSystem() const { return mSystem; }

    JSValue playerPrototype() const { return mPlayerPrototype; }

    JSClassID playerClassId() const { return mPlayerClassId; }

    static ScriptApi *fromRuntime(JSRuntime *runtime);

private:
    void _registerModule();

    void _registerUiModule();

    void _installUiBootstrap();

    void _installDataBootstrap();

    void _resolveForm(uint32_t formId, const std::string &response, bool cancelled);

    void _buildSystem();

    void _buildWorld();

    void _buildActorClass();

    void _buildBlockClass();

    void _buildItemStackClass();

    void _buildEvents();

    void _subscribeGameEvents();

    void _dispatch(ScriptEvent event, JSValue eventObject);

    bool _dispatchCancellable(ScriptEvent event, JSValue eventObject);

    void _emitInventoryChanges();

    JSContext *mContext;
    JSRuntime *mRuntime;
    ServerNetworkHandler &mHost;

    struct SteppedBlock {
        std::string mIdentifier;
        Vector3i mPosition;
    };

    // the custom block each actor stands on: fires once per step, not per tick
    std::unordered_map<int64_t, SteppedBlock> mStepping;
    std::mt19937 mTickRandom{std::random_device{}()};
    ScriptScheduler mScheduler;
    PlayerHandleRegistry mPlayers;

    JSClassID mPlayerClassId;
    JSValue mPlayerPrototype;
    JSClassID mActorClassId;
    JSValue mActorPrototype;
    JSClassID mBlockClassId;
    JSValue mBlockPrototype;
    JSClassID mItemStackClassId;
    JSValue mItemStackPrototype;
    JSValue mWorld;
    JSValue mSystem;
    JSValue mAfterEvents;
    JSValue mBeforeEvents;

    std::vector<JSValue> mSubscribers[(int) ScriptEvent::Count];
    std::unordered_map<std::string, std::vector<JSValue>> mNamedSubscribers;
    std::unordered_map<uint64_t, std::vector<ItemStack>> mInventorySnapshots;

    uint32_t mNextFormId;
    std::unordered_map<uint32_t, JSValue> mFormResolvers;
};
