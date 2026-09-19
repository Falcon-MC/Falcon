#pragma once

#include "Level/Dimension.h"
#include "Scripting/Binding/PlayerHandleRegistry.h"
#include "Scripting/ScriptScheduler.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <quickjs.h>

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

    bool setBlockType(DimensionType dimension, int32_t x, int32_t y, int32_t z, const std::string &typeId);

    int32_t runCommandAsPlayer(ServerPlayer &player, const std::string &commandLine);

    int32_t runCommandAsConsole(const std::string &commandLine);

    JSValue showForm(JSValueConst playerValue, const std::string &formData);

    JSValue makeActor(ServerActor &actor);

    ServerActor *resolveActor(JSValueConst value);

    JSClassID actorClassId() const { return mActorClassId; }

    JSValue makeBlock(DimensionType dimension, int32_t x, int32_t y, int32_t z);

    JSValue makeItemStack(const std::string &typeId, int32_t amount);

    JSValue makeHeldItemStack(ServerPlayer &player, const std::string &identifier);

    JSValue makeDimension(DimensionType dimension);

    /** The dimension a script Dimension object stands for, read from its `id`. */
    static DimensionType dimensionOf(JSContext *ctx, JSValueConst dimension);

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

    void emitWorldInitialize();

    void fireHeldItemComponents(ServerPlayer &player, const char *hook, JSValue event);

    JSValue moduleWorld() const { return mWorld; }

    JSValue moduleSystem() const { return mSystem; }

    JSValue playerPrototype() const { return mPlayerPrototype; }

    JSClassID playerClassId() const { return mPlayerClassId; }

    static ScriptApi *fromRuntime(JSRuntime *runtime);

private:
    void _registerModule();

    void _registerUiModule();

    void _installUiBootstrap();

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

    JSContext *mContext;
    JSRuntime *mRuntime;
    ServerNetworkHandler &mHost;
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

    uint32_t mNextFormId;
    std::unordered_map<uint32_t, JSValue> mFormResolvers;
};
