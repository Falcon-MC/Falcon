#pragma once

#include "Core/Math/Vector3i.h"
#include "World/Level/Dimension/Dimension.h"
#include "Protocol/Types/ItemStack.h"
#include "Scripting/Binding/PlayerHandleRegistry.h"
#include "Scripting/Binding/ScriptNatives.h"
#include "Scripting/Binding/ScriptSurface.h"
#include "Scripting/ScriptModules.h"
#include "Scripting/ScriptScheduler.h"

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include <quickjs.h>

class Actor;
class ItemActor;
class Level;
class ServerNetworkHandler;
class ServerPlayer;
class ServerActor;

// what the running script may touch: early execution before startup ends, restricted inside before events
enum class ScriptPrivilege { Normal, Early, Restricted };

// one behavior pack's scripts: its own context, module versions and official surface
class ScriptApi {
public:
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

    const std::string &packName() const {
        return mPackName;
    }

    const std::string &packRoot() const {
        return mPackRoot;
    }

    const ScriptModuleSet &modules() const {
        return mModules;
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

    // an event object of the class the signal fires
    JSValue makeEvent(const std::string &signalKey);

    JSValue throwError(const std::string &errorClass, const std::string &message);

    JSValue throwInvalidEntity(const char *action, const std::string &member, const ScriptEntityRef &entity);

    JSValue makePlayer(ServerPlayer &player);

    JSValue makeActor(ServerActor &actor);

    JSValue makeEntity(Actor &actor);

    ServerPlayer *resolvePlayer(JSValueConst value);

    ServerPlayer *resolvePlayerByHandle(uint32_t handle);

    ServerActor *resolveActor(JSValueConst value);

    Actor *resolveEntity(JSValueConst value);

    Actor *resolveEntity(const ScriptEntityRef &ref);

    JSValue makeItem(const ItemStack &item);

    JSValue makeItemStack(const std::string &typeId, int32_t amount);

    JSValue makeHeldItemStack(ServerPlayer &player, const std::string &identifier);

    JSValue makeBlock(const Level &level, int32_t x, int32_t y, int32_t z);

    // default states: the events only carry the block name
    JSValue makePermutation(const std::string &identifier);

    JSValue makeDimension(const Level &level);

    Level &levelOf(JSValueConst dimension);

    // falls back to the default world once unloaded
    Level &resolveLevel(uint32_t worldId, DimensionType dimension);

    bool giveItem(ServerPlayer &player, const std::string &typeId, int32_t amount);

    bool setBlockType(Level &level, int32_t x, int32_t y, int32_t z, const std::string &typeId);

    int32_t runCommandAsPlayer(ServerPlayer &player, const std::string &commandLine);

    int32_t runCommandAsConsole(const std::string &commandLine);

    JSValue showForm(JSValueConst playerValue, const std::string &formData);

    void addSubscriber(const std::string &key, JSValue callback);

    void removeSubscriber(const std::string &key, JSValueConst callback);

    bool hasSubscribers(const std::string &key) const;

    // before events run restricted, startup early; true when a subscriber set cancel
    bool emit(const std::string &key, JSValue eventObject);

    bool registerCustomComponent(bool blocks, const std::string &name, JSValueConst handlers);

    void fireBlockComponents(const std::string &blockId, const char *hook, JSValue event);

    void fireHeldItemComponents(ServerPlayer &player, const char *hook, JSValue event);

    void startup();

    void shutdown();

    void forgetEntity(int64_t uniqueId);

    void forgetPlayer(ServerPlayer &player);

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

    JSValue moduleScoreboard() const {
        return mScoreboard;
    }

    JSValue moduleTickingAreas() const {
        return mTickingAreas;
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

    bool propertyMagic(JSValueConst target, const char *cls, const char *name, JSCFunctionMagic *getter, int magic);

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

    void _bindSystem();

    void _bindEntity();

    void _bindPlayer();

    void _bindComponents();

    void _bindBlock();

    void _bindItemStack();

    void _bindDimension();

    void _bindEvents();

    void _bindRegistries();

    void _installBootstraps();

    void _defineEnums();

    void _registerModules();

    void _subscribeGameEvents();

    void _emitInventoryChanges();

    void _resolveForm(uint32_t formId, const std::string &response, bool cancelled);

    JSValue _makeBlockEvent(const Level &level, const Vector3i &position);

    void _fireComponents(std::unordered_map<std::string, JSValue> &handlers, const std::vector<std::string> *components,
                         const char *hook, JSValue event);

    JSValue _makeSignalHolder(const char *className, const char *signalOwner, const char *keyPrefix);

    JSContext *mContext;
    ServerNetworkHandler &mHost;
    JSClassID mNativeClassId;
    std::string mPackName;
    std::string mPackRoot;
    ScriptModuleSet mModules;
    ScriptApiTrack mTrack;
    ScriptSurface mSurface;
    ScriptSurface mUiSurface;
    ScriptSurface mCommonSurface;
    ScriptPrivilege mPrivilege = ScriptPrivilege::Normal;

    struct SteppedBlock {
        std::string mIdentifier;
        Vector3i mPosition;
    };

    // the custom block each actor stands on: fires once per step, not per tick
    std::unordered_map<int64_t, SteppedBlock> mStepping;
    ScriptScheduler mScheduler;
    PlayerHandleRegistry &mPlayers;

    // a deque: a native may define a class, and so a binding, while its own binding is in use
    std::deque<Binding> mBindings;
    std::unordered_map<std::string, ScriptClass> mClasses;
    std::vector<std::pair<std::string, JSValue>> mServerExports;
    std::vector<std::pair<std::string, JSValue>> mUiExports;
    std::vector<std::pair<std::string, JSValue>> mCommonExports;

    // same entity, same object: scripts compare and key maps by them
    std::unordered_map<int64_t, JSValue> mActorObjects;
    std::unordered_map<uint32_t, JSValue> mPlayerObjects;
    std::unordered_map<uint64_t, JSValue> mDimensionObjects;

    JSValue mWorld = JS_UNDEFINED;
    JSValue mSystem = JS_UNDEFINED;
    JSValue mAfterEvents = JS_UNDEFINED;
    JSValue mBeforeEvents = JS_UNDEFINED;
    JSValue mSystemAfterEvents = JS_UNDEFINED;
    JSValue mSystemBeforeEvents = JS_UNDEFINED;
    JSValue mScoreboard = JS_UNDEFINED;
    JSValue mTickingAreas = JS_UNDEFINED;
    JSValue mErrors = JS_UNDEFINED;
    JSValue mBlockRegistry = JS_UNDEFINED;
    JSValue mItemRegistry = JS_UNDEFINED;

    std::unordered_map<std::string, std::vector<JSValue>> mSubscribers;
    std::unordered_map<std::string, JSValue> mBlockComponents;
    std::unordered_map<std::string, JSValue> mItemComponents;
    bool mRegistrationOpen = false;
    std::unordered_map<uint64_t, std::vector<ItemStack>> mInventorySnapshots;

    uint32_t mNextFormId = 1;
    std::unordered_map<uint32_t, JSValue> mFormResolvers;
};
