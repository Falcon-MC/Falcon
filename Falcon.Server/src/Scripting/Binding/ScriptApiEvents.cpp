#include "Scripting/Binding/ScriptApi.h"

#include "World/Actor/ServerActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Event/EventBus.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>

using namespace ScriptValues;

namespace {
    JSValue hitGetter(JSContext *ctx, JSValueConst, int, JSValueConst *, int, JSValue *data) {
        return JS_DupValue(ctx, data[0]);
    }

    // getBlockHit and friends: the hit is a closure value, not a property scripts could read
    JSValue makeHitGetter(JSContext *ctx, const char *name, JSValue hit) {
        JSValue getter = JS_NewCFunctionData(ctx, hitGetter, 0, 0, 1, &hit);
        JS_FreeValue(ctx, hit);
        JS_DefinePropertyValueStr(ctx, getter, "name", JS_NewString(ctx, name), JS_PROP_CONFIGURABLE);
        return getter;
    }

    JSValue makeDamageSource(JSContext *ctx, const std::string &cause) {
        JSValue source = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, source, "cause", string(ctx, cause));
        return source;
    }
}

void ScriptApi::install() {
    _bindEntity();
    _bindPlayer();
    _bindComponents();
    _bindBlock();
    _bindItemStack();
    _bindDimension();
    _bindEvents();
    _bindRegistries();
    _bindSystem();
    _bindWorld();
    _installBootstraps();
    _defineEnums();
    _registerModules();
    _subscribeGameEvents();
}

void ScriptApi::tick(int64_t currentTick) {
    mScheduler.tick(currentTick);
    _emitInventoryChanges();
}

JSValue ScriptApi::showForm(JSValueConst playerValue, const std::string &formData) {
    JSValue resolvingFuncs[2];
    JSValue promise = JS_NewPromiseCapability(mContext, resolvingFuncs);

    ServerPlayer *player = resolvePlayer(playerValue);
    if (player == nullptr) {
        JSValue result = JS_NewObject(mContext);
        JS_SetPropertyStr(mContext, result, "canceled", JS_NewBool(mContext, true));
        JSValue callResult = JS_Call(mContext, resolvingFuncs[0], JS_UNDEFINED, 1, &result);
        JS_FreeValue(mContext, callResult);
        JS_FreeValue(mContext, result);
        JS_FreeValue(mContext, resolvingFuncs[0]);
        JS_FreeValue(mContext, resolvingFuncs[1]);
        return promise;
    }

    const uint32_t formId = mNextFormId++;
    mFormResolvers[formId] = resolvingFuncs[0];
    JS_FreeValue(mContext, resolvingFuncs[1]);

    mHost.sendModalForm(*player, formId, formData,
                        [this, formId](ServerPlayer &, const std::string &response, bool cancelled) {
                            _resolveForm(formId, response, cancelled);
                        });
    return promise;
}

void ScriptApi::_resolveForm(uint32_t formId, const std::string &response, bool cancelled) {
    auto it = mFormResolvers.find(formId);
    if (it == mFormResolvers.end())
        return;

    JSValue resolve = it->second;
    mFormResolvers.erase(it);

    JSValue result = JS_NewObject(mContext);
    JS_SetPropertyStr(mContext, result, "canceled", JS_NewBool(mContext, cancelled));

    JSValue value = JS_NULL;
    if (!cancelled && !response.empty()) {
        value = JS_ParseJSON(mContext, response.c_str(), response.size(), "<form-response>");
        if (JS_IsException(value)) {
            JS_FreeValue(mContext, JS_GetException(mContext));
            value = JS_NULL;
        }
    }
    JS_SetPropertyStr(mContext, result, "value", value);

    JSValue callResult = JS_Call(mContext, resolve, JS_UNDEFINED, 1, &result);
    JS_FreeValue(mContext, callResult);
    JS_FreeValue(mContext, result);
    JS_FreeValue(mContext, resolve);
}

JSValue ScriptApi::_makeBlockEvent(const Level &level, const Vector3i &position) {
    JSValue event = JS_NewObject(mContext);
    JS_SetPropertyStr(mContext, event, "block", makeBlock(level, position.x, position.y, position.z));
    JS_SetPropertyStr(mContext, event, "dimension", makeDimension(level));
    return event;
}

JSValue ScriptApi::makePermutation(const std::string &identifier) {
    for (const auto &entry: mServerExports) {
        if (entry.first != "BlockPermutation")
            continue;

        JSValue resolve = JS_GetPropertyStr(mContext, entry.second, "resolve");
        JSValue id = string(mContext, identifier);
        JSValue permutation = JS_Call(mContext, resolve, entry.second, 1, &id);
        JS_FreeValue(mContext, id);
        JS_FreeValue(mContext, resolve);
        if (!JS_IsException(permutation))
            return permutation;
        JS_FreeValue(mContext, JS_GetException(mContext));
        break;
    }
    return JS_UNDEFINED;
}

void ScriptApi::emitProjectileHitBlock(ServerActor &projectile, int32_t x, int32_t y, int32_t z) {
    if (!hasSubscribers("after.projectileHitBlock"))
        return;

    JSValue event = makeEvent("WorldAfterEvents.projectileHitBlock");
    JS_SetPropertyStr(mContext, event, "projectile", makeActor(projectile));
    JS_SetPropertyStr(mContext, event, "dimension", makeDimension(mHost.getLevelFor(projectile)));

    if (projectile.hasOwnerPlayer()) {
        ServerPlayer *owner = resolvePlayerByHandle(projectile.getOwnerPlayerHandle());
        if (owner != nullptr)
            JS_SetPropertyStr(mContext, event, "source", makePlayer(*owner));
    } else if (projectile.getOwnerUniqueId() >= 0) {
        ServerActor *owner = mHost.getActor(projectile.getOwnerUniqueId());
        if (owner != nullptr)
            JS_SetPropertyStr(mContext, event, "source", makeActor(*owner));
    }

    JSValue hit = JS_NewObject(mContext);
    JS_SetPropertyStr(mContext, hit, "block", makeBlock(mHost.getLevelFor(projectile), x, y, z));
    JS_SetPropertyStr(mContext, hit, "faceLocation",
                      vector3(mContext, (float) x + 0.5f, (float) y + 0.5f, (float) z + 0.5f));

    // TODO: the contact face from the collision, the motion's dominant axis for now
    const Vector3f &motion = projectile.getMotion();
    const float absX = std::fabs(motion.x);
    const float absY = std::fabs(motion.y);
    const float absZ = std::fabs(motion.z);
    const char *face;
    if (absY >= absX && absY >= absZ)
        face = motion.y < 0.0f ? "Up" : "Down";
    else if (absX >= absZ)
        face = motion.x > 0.0f ? "West" : "East";
    else
        face = motion.z > 0.0f ? "North" : "South";
    JS_SetPropertyStr(mContext, hit, "face", JS_NewString(mContext, face));

    JS_SetPropertyStr(mContext, event, "getBlockHit", makeHitGetter(mContext, "getBlockHit", hit));
    emit("after.projectileHitBlock", event);
}

void ScriptApi::emitItemUseOnBlock(ServerPlayer &player, int32_t x, int32_t y, int32_t z) {
    const ItemStack &held = player.getInventory().getItemInHand();
    if (held.isAir() || held.mDefinition == nullptr)
        return;

    const std::string typeId = held.mDefinition->getIdentifier();
    JSValue event = JS_NewObject(mContext);
    JS_SetPropertyStr(mContext, event, "source", makePlayer(player));
    JS_SetPropertyStr(mContext, event, "block", makeBlock(mHost.getLevelFor(player), x, y, z));
    JS_SetPropertyStr(mContext, event, "itemStack", makeHeldItemStack(player, typeId));

    if (hasSubscribers("before.itemUseOn")) {
        JSValue before = makeEvent("WorldBeforeEvents.itemUseOn");
        JS_SetPropertyStr(mContext, before, "source", makePlayer(player));
        JS_SetPropertyStr(mContext, before, "block", makeBlock(mHost.getLevelFor(player), x, y, z));
        JS_SetPropertyStr(mContext, before, "itemStack", makeHeldItemStack(player, typeId));
        JS_SetPropertyStr(mContext, before, "cancel", JS_FALSE);
        emit("before.itemUseOn", before);
    }

    fireHeldItemComponents(player, "onUseOn", event);
}

void ScriptApi::emitBlockInteract(ServerPlayer &player, const std::string &blockId, const Vector3i &position,
                                  const char *face) {
    JSValue event = _makeBlockEvent(mHost.getLevelFor(player), position);
    JS_SetPropertyStr(mContext, event, "player", makePlayer(player));
    JS_SetPropertyStr(mContext, event, "face", string(mContext, face));
    fireBlockComponents(blockId, "onPlayerInteract", event);
}

void ScriptApi::emitBlockTick(Level &level, const std::string &blockId, const Vector3i &position) {
    fireBlockComponents(blockId, "onTick", _makeBlockEvent(level, position));
}

void ScriptApi::emitBlockStep(Actor &actor, const Level &level, const std::string &blockId, const Vector3i &position) {
    const auto previous = mStepping.find(actor.getUniqueId());
    if (previous != mStepping.end() && previous->second.mPosition.x == position.x
        && previous->second.mPosition.y == position.y && previous->second.mPosition.z == position.z)
        return;

    emitBlockStepOff(actor, level);
    mStepping[actor.getUniqueId()] = {blockId, position};

    JSValue event = _makeBlockEvent(level, position);
    JS_SetPropertyStr(mContext, event, "entity", makeEntity(actor));
    fireBlockComponents(blockId, "onStepOn", event);
}

void ScriptApi::emitBlockStepOff(Actor &actor, const Level &level) {
    const auto previous = mStepping.find(actor.getUniqueId());
    if (previous == mStepping.end())
        return;

    const SteppedBlock left = previous->second;
    mStepping.erase(previous);

    JSValue event = _makeBlockEvent(level, left.mPosition);
    JS_SetPropertyStr(mContext, event, "entity", makeEntity(actor));
    fireBlockComponents(left.mIdentifier, "onStepOff", event);
}

void ScriptApi::_subscribeGameEvents() {
    EventBus &bus = mHost.getEventBus();

    bus.after().mPlayerBreakBlock.subscribe([this](PlayerBreakBlockAfterEvent &event) {
        Level &level = mHost.getLevelFor(event.mPlayer);
        if (hasSubscribers("after.playerBreakBlock")) {
            JSValue object = makeEvent("WorldAfterEvents.playerBreakBlock");
            JS_SetPropertyStr(mContext, object, "player", makePlayer(event.mPlayer));
            JS_SetPropertyStr(mContext, object, "block", makeBlock(level, event.mBlockPosition.x,
                                                                    event.mBlockPosition.y, event.mBlockPosition.z));
            JS_SetPropertyStr(mContext, object, "dimension", makeDimension(level));
            JS_SetPropertyStr(mContext, object, "brokenBlockPermutation",
                              makePermutation(event.mBrokenBlockIdentifier));
            emit("after.playerBreakBlock", object);
        }

        JSValue mineEvent = JS_NewObject(mContext);
        JS_SetPropertyStr(mContext, mineEvent, "source", makePlayer(event.mPlayer));
        JS_SetPropertyStr(mContext, mineEvent, "block", makeBlock(level, event.mBlockPosition.x,
                                                                   event.mBlockPosition.y, event.mBlockPosition.z));
        JS_SetPropertyStr(mContext, mineEvent, "minedBlockPermutation", makePermutation(event.mBrokenBlockIdentifier));
        fireHeldItemComponents(event.mPlayer, "onMineBlock", mineEvent);

        // the hook was renamed with the 2.0 module
        JSValue breakEvent = _makeBlockEvent(level, event.mBlockPosition);
        JS_SetPropertyStr(mContext, breakEvent, "player", makePlayer(event.mPlayer));
        JS_SetPropertyStr(mContext, breakEvent, "brokenBlockPermutation", makePermutation(event.mBrokenBlockIdentifier));
        fireBlockComponents(event.mBrokenBlockIdentifier, isStable1() ? "onPlayerDestroy" : "onPlayerBreak",
                            breakEvent);
    });

    bus.after().mPlayerPlaceBlock.subscribe([this](PlayerPlaceBlockAfterEvent &event) {
        Level &level = mHost.getLevelFor(event.mPlayer);
        if (hasSubscribers("after.playerPlaceBlock")) {
            JSValue object = makeEvent("WorldAfterEvents.playerPlaceBlock");
            JS_SetPropertyStr(mContext, object, "player", makePlayer(event.mPlayer));
            JS_SetPropertyStr(mContext, object, "block", makeBlock(level, event.mBlockPosition.x,
                                                                    event.mBlockPosition.y, event.mBlockPosition.z));
            JS_SetPropertyStr(mContext, object, "dimension", makeDimension(level));
            emit("after.playerPlaceBlock", object);
        }

        fireBlockComponents(event.mBlockIdentifier, "onPlace", _makeBlockEvent(level, event.mBlockPosition));
    });

    bus.after().mPlayerSpawn.subscribe([this](PlayerSpawnAfterEvent &event) {
        if (!hasSubscribers("after.playerSpawn"))
            return;
        JSValue object = makeEvent("WorldAfterEvents.playerSpawn");
        JS_SetPropertyStr(mContext, object, "player", makePlayer(event.mPlayer));
        JS_SetPropertyStr(mContext, object, "initialSpawn", JS_NewBool(mContext, event.mInitialSpawn));
        emit("after.playerSpawn", object);
    });

    bus.after().mPlayerJoin.subscribe([this](PlayerJoinAfterEvent &event) {
        if (!hasSubscribers("after.playerJoin"))
            return;
        JSValue object = makeEvent("WorldAfterEvents.playerJoin");
        JS_SetPropertyStr(mContext, object, "playerId", string(mContext, std::to_string(event.mPlayer.getUniqueId())));
        JS_SetPropertyStr(mContext, object, "playerName", string(mContext, event.mPlayer.getName()));
        emit("after.playerJoin", object);
    });

    bus.after().mPlayerLeave.subscribe([this](PlayerLeaveAfterEvent &event) {
        if (hasSubscribers("after.playerLeave")) {
            JSValue object = makeEvent("WorldAfterEvents.playerLeave");
            const std::string id = event.mPlayer == nullptr ? std::string()
                                                            : std::to_string(event.mPlayer->getUniqueId());
            JS_SetPropertyStr(mContext, object, "playerId", string(mContext, id));
            JS_SetPropertyStr(mContext, object, "playerName", string(mContext, event.mPlayerName));
            emit("after.playerLeave", object);
        }
        if (event.mPlayer != nullptr)
            forgetPlayer(*event.mPlayer);
    });

    bus.after().mChatSend.subscribe([this](PlayerChatAfterEvent &event) {
        if (!hasSubscribers("after.chatSend"))
            return;
        JSValue object = makeEvent("WorldAfterEvents.chatSend");
        JS_SetPropertyStr(mContext, object, "sender", makePlayer(event.mSender));
        JS_SetPropertyStr(mContext, object, "message", string(mContext, event.mMessage));
        emit("after.chatSend", object);
    });

    bus.after().mEntityHurt.subscribe([this](EntityHurtAfterEvent &event) {
        if (!hasSubscribers("after.entityHurt"))
            return;
        JSValue object = makeEvent("WorldAfterEvents.entityHurt");
        JS_SetPropertyStr(mContext, object, "hurtEntity", makePlayer(event.mHurtEntity));
        JS_SetPropertyStr(mContext, object, "damage", JS_NewFloat64(mContext, event.mDamage));
        JS_SetPropertyStr(mContext, object, "damageSource", makeDamageSource(mContext, event.mCause));
        emit("after.entityHurt", object);
    });

    bus.after().mEntityDie.subscribe([this](EntityDieAfterEvent &event) {
        if (!hasSubscribers("after.entityDie"))
            return;
        JSValue object = makeEvent("WorldAfterEvents.entityDie");
        JS_SetPropertyStr(mContext, object, "deadEntity", makePlayer(event.mDeadEntity));
        JS_SetPropertyStr(mContext, object, "damageSource", makeDamageSource(mContext, event.mCause));
        emit("after.entityDie", object);
    });

    bus.after().mItemUse.subscribe([this](ItemUseAfterEvent &event) {
        if (!hasSubscribers("after.itemUse"))
            return;
        JSValue object = makeEvent("WorldAfterEvents.itemUse");
        JS_SetPropertyStr(mContext, object, "itemStack", makeHeldItemStack(event.mPlayer, event.mItemIdentifier));
        JS_SetPropertyStr(mContext, object, "source", makePlayer(event.mPlayer));
        emit("after.itemUse", object);
    });

    bus.after().mPlayerHotbarSelectedSlotChange.subscribe([this](PlayerHotbarSelectedSlotChangeAfterEvent &event) {
        if (!hasSubscribers("after.playerHotbarSelectedSlotChange"))
            return;
        JSValue object = makeEvent("WorldAfterEvents.playerHotbarSelectedSlotChange");
        JS_SetPropertyStr(mContext, object, "player", makePlayer(event.mPlayer));
        JS_SetPropertyStr(mContext, object, "newSlotSelected", JS_NewInt32(mContext, event.mNewSlotSelected));
        JS_SetPropertyStr(mContext, object, "previousSlotSelected", JS_NewInt32(mContext, event.mPreviousSlotSelected));
        JS_SetPropertyStr(mContext, object, "itemStack", makeItem(event.mPlayer.getInventory().getItemInHand()));
        emit("after.playerHotbarSelectedSlotChange", object);
    });

    bus.before().mPlayerBreakBlock.subscribe([this](PlayerBreakBlockBeforeEvent &event) {
        if (!hasSubscribers("before.playerBreakBlock"))
            return;
        Level &level = mHost.getLevelFor(event.mPlayer);
        JSValue object = makeEvent("WorldBeforeEvents.playerBreakBlock");
        JS_SetPropertyStr(mContext, object, "player", makePlayer(event.mPlayer));
        JS_SetPropertyStr(mContext, object, "block", makeBlock(level, event.mBlockPosition.x, event.mBlockPosition.y,
                                                                event.mBlockPosition.z));
        JS_SetPropertyStr(mContext, object, "dimension", makeDimension(level));
        JS_SetPropertyStr(mContext, object, "cancel", JS_FALSE);
        if (emit("before.playerBreakBlock", object))
            event.cancel();
    });

    bus.before().mChatSend.subscribe([this](PlayerChatBeforeEvent &event) {
        if (!hasSubscribers("before.chatSend"))
            return;
        JSValue object = makeEvent("WorldBeforeEvents.chatSend");
        JS_SetPropertyStr(mContext, object, "sender", makePlayer(event.mSender));
        JS_SetPropertyStr(mContext, object, "message", string(mContext, event.mMessage));
        JS_SetPropertyStr(mContext, object, "cancel", JS_FALSE);
        if (emit("before.chatSend", object))
            event.cancel();
    });
}

void ScriptApi::_emitInventoryChanges() {
    if (!hasSubscribers("after.playerInventoryItemChange")) {
        mInventorySnapshots.clear();
        return;
    }

    std::unordered_map<uint64_t, std::vector<ItemStack>> current;

    for (auto &entry: mHost.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned())
            continue;

        const std::vector<ItemStack> &contents = player.getInventory().getContents();
        const uint64_t key = player.getRuntimeId();
        auto previous = mInventorySnapshots.find(key);

        if (previous != mInventorySnapshots.end()) {
            for (size_t slot = 0; slot < contents.size() && slot < previous->second.size(); ++slot) {
                const ItemStack &before = previous->second[slot];
                const ItemStack &after = contents[slot];
                const bool same = before.mDefinition == after.mDefinition && before.mCount == after.mCount
                                  && before.mDamage == after.mDamage && before.mTag == after.mTag;
                if (same)
                    continue;

                JSValue event = makeEvent("WorldAfterEvents.playerInventoryItemChange");
                JS_SetPropertyStr(mContext, event, "player", makePlayer(player));
                JS_SetPropertyStr(mContext, event, "slot", JS_NewInt32(mContext, (int32_t) slot));
                JS_SetPropertyStr(mContext, event, "inventoryType", JS_NewString(mContext, "Inventory"));
                JS_SetPropertyStr(mContext, event, "itemStack", makeItem(after));
                JS_SetPropertyStr(mContext, event, "beforeItemStack", makeItem(before));
                emit("after.playerInventoryItemChange", event);
            }
        }

        current[key] = contents;
    }

    mInventorySnapshots = std::move(current);
}
