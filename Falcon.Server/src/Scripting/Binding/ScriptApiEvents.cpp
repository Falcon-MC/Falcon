#include "Scripting/Binding/ScriptApi.h"

#include "World/Actor/ServerActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Event/EventBus.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Item/CraftingRecipeTable.h"

#include <algorithm>
#include <cmath>
#include <memory>

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

    // the depth the protocol accepts for a change value
    const int MAX_STORE_DEPTH = 16;

    // numbers are doubles and bigints longs, as the store's variant keeps them
    bool toStoreValue(JSContext *ctx, JSValueConst value, DataStoreChangeValue &out, int depth) {
        if (depth > MAX_STORE_DEPTH)
            return false;
        if (JS_IsBool(value)) {
            out.mType = DataStoreChangeValueType::Bool;
            out.mBoolValue = JS_ToBool(ctx, value) == 1;
            return true;
        }
        if (JS_IsBigInt(ctx, value)) {
            out.mType = DataStoreChangeValueType::Long;
            return JS_ToBigInt64(ctx, &out.mLongValue, value) == 0;
        }
        if (JS_IsNumber(value)) {
            out.mType = DataStoreChangeValueType::Double;
            return JS_ToFloat64(ctx, &out.mDoubleValue, value) == 0;
        }
        if (JS_IsString(value)) {
            out.mType = DataStoreChangeValueType::String;
            out.mStringValue = toString(ctx, value);
            return true;
        }
        if (JS_IsArray(value)) {
            out.mType = DataStoreChangeValueType::Array;
            uint32_t length = 0;
            JSValue lengthValue = JS_GetPropertyStr(ctx, value, "length");
            JS_ToUint32(ctx, &length, lengthValue);
            JS_FreeValue(ctx, lengthValue);
            for (uint32_t i = 0; i < length; i++) {
                JSValue element = JS_GetPropertyUint32(ctx, value, i);
                DataStoreChangeValue child;
                const bool ok = toStoreValue(ctx, element, child, depth + 1);
                JS_FreeValue(ctx, element);
                if (!ok)
                    return false;
                out.mArrayValue.push_back(std::move(child));
            }
            return true;
        }
        if (!JS_IsObject(value)) {
            out.mType = DataStoreChangeValueType::Null;
            return true;
        }

        JSPropertyEnum *properties = nullptr;
        uint32_t count = 0;
        if (JS_GetOwnPropertyNames(ctx, &properties, &count, value, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) != 0)
            return false;
        out.mType = DataStoreChangeValueType::Map;
        bool ok = true;
        for (uint32_t i = 0; i < count && ok; i++) {
            JSValue child = JS_GetProperty(ctx, value, properties[i].atom);
            const char *key = JS_AtomToCString(ctx, properties[i].atom);
            DataStoreChangeValue converted;
            ok = key != nullptr && toStoreValue(ctx, child, converted, depth + 1);
            if (ok)
                out.mMapValue.emplace_back(key, std::move(converted));
            if (key != nullptr)
                JS_FreeCString(ctx, key);
            JS_FreeValue(ctx, child);
        }
        JS_FreePropertyEnum(ctx, properties, count);
        return ok;
    }

    std::vector<std::string> toPathList(JSContext *ctx, JSValueConst list) {
        std::vector<std::string> paths;
        if (!JS_IsArray(list))
            return paths;
        uint32_t length = 0;
        JSValue lengthValue = JS_GetPropertyStr(ctx, list, "length");
        JS_ToUint32(ctx, &length, lengthValue);
        JS_FreeValue(ctx, lengthValue);
        for (uint32_t i = 0; i < length; i++) {
            JSValue entry = JS_GetPropertyUint32(ctx, list, i);
            paths.push_back(toString(ctx, entry));
            JS_FreeValue(ctx, entry);
        }
        return paths;
    }

    JSValue scalarToJs(JSContext *ctx, const DataStoreScalar &scalar) {
        switch (scalar.mType) {
            case DataStoreScalarType::Number:
                return JS_NewFloat64(ctx, scalar.mNumberValue);
            case DataStoreScalarType::Bool:
                return JS_NewBool(ctx, scalar.mBoolValue);
            case DataStoreScalarType::String:
                return string(ctx, scalar.mStringValue);
            default:
                return JS_UNDEFINED;
        }
    }

    const char *outcomeName(ServerNetworkHandler::DataDrivenScreenOutcome outcome) {
        switch (outcome) {
            case ServerNetworkHandler::DataDrivenScreenOutcome::ProgrammaticClose:
                return "ProgrammaticClose";
            case ServerNetworkHandler::DataDrivenScreenOutcome::ProgrammaticCloseAll:
                return "ProgrammaticCloseAll";
            case ServerNetworkHandler::DataDrivenScreenOutcome::ClientCanceled:
                return "ClientCanceled";
            case ServerNetworkHandler::DataDrivenScreenOutcome::UserBusy:
                return "UserBusy";
            case ServerNetworkHandler::DataDrivenScreenOutcome::InvalidForm:
                return "InvalidForm";
            case ServerNetworkHandler::DataDrivenScreenOutcome::PlayerLeft:
                return "PlayerLeft";
        }
        return "InvalidForm";
    }
}

void ScriptApi::install() {
    _bindEntity();
    _bindPlayer();
    _bindComponents();
    _bindEntityComponents();
    _bindBlock();
    _bindItemStack();
    _bindContainers();
    _bindDimension();
    _bindEvents();
    _bindRegistries();
    _bindSystem();
    _bindWorld();
    _bindStructures();
    _bindClocks();
    _bindScoreboard();
    _bindWaypoints();
    _bindSounds();
    _bindCommands();
    _installBootstraps();
    _defineEnums();
    _registerModules();
    _subscribeGameEvents();
}

void ScriptApi::tick(int64_t currentTick) {
    mScheduler.tick(currentTick);
    for (size_t index = 0; index < mTickJobs.size();) {
        if (mTickJobs[index]())
            ++index;
        else
            mTickJobs.erase(mTickJobs.begin() + (std::ptrdiff_t) index);
    }
    if (!mPendingTickingAreas.empty())
        resolveTickingAreas();
    if (!mPendingFilteredTexts.empty())
        _resolveFilteredTexts();
    _emitInventoryChanges();
    _emitCursorChanges();
    _tickLocatorBars();
    _tickSounds();
}

JSValue ScriptApi::showForm(JSValueConst playerValue, const std::string &formData) {
    JSValue resolvingFuncs[2];
    JSValue promise = JS_NewPromiseCapability(mContext, resolvingFuncs);

    const uint32_t formId = mNextFormId++;
    mFormResolvers[formId] = resolvingFuncs[0];
    JS_FreeValue(mContext, resolvingFuncs[1]);

    ServerPlayer *player = resolvePlayer(playerValue);
    if (player == nullptr) {
        _resolveForm(formId, std::string(), "PlayerQuit");
        return promise;
    }

    mHost.sendModalForm(*player, formId, formData,
                        [this, formId](ServerPlayer &, const std::string &response,
                                       ServerNetworkHandler::ModalFormOutcome outcome) {
                            switch (outcome) {
                                case ServerNetworkHandler::ModalFormOutcome::Answered:
                                    _resolveForm(formId, response, "Answered");
                                    break;
                                case ServerNetworkHandler::ModalFormOutcome::UserClosed:
                                    _resolveForm(formId, response, "UserClosed");
                                    break;
                                case ServerNetworkHandler::ModalFormOutcome::UserBusy:
                                    _resolveForm(formId, response, "UserBusy");
                                    break;
                                case ServerNetworkHandler::ModalFormOutcome::PlayerQuit:
                                    _resolveForm(formId, response, "PlayerQuit");
                                    break;
                            }
                        });
    return promise;
}

void ScriptApi::closeAllForms(JSValueConst playerValue) {
    ServerPlayer *player = resolvePlayer(playerValue);
    if (player != nullptr)
        mHost.closeAllForms(*player);
}

JSValue ScriptApi::showScreen(JSValueConst playerValue, const std::string &screenId, const std::string &propertyPrefix,
                              JSValueConst value, JSValueConst writable, JSValueConst onWrite, JSValueConst onClose) {
    ServerPlayer *player = resolvePlayer(playerValue);
    if (player == nullptr)
        return JS_NewInt32(mContext, 0);

    DataStoreChangeValue storeValue;
    if (!toStoreValue(mContext, value, storeValue, 0))
        return JS_ThrowTypeError(mContext, "Native type conversion failed.");

    JSValue writeCallback = JS_DupValue(mContext, onWrite);
    JSValue closeCallback = JS_DupValue(mContext, onClose);
    std::shared_ptr<bool> open = std::make_shared<bool>(true);
    ServerNetworkHandler::DataDrivenScreenCallbacks callbacks;
    callbacks.mOnWrite = [this, writeCallback, open](const std::string &path, const DataStoreScalar &scalar) {
        if (!*open)
            return;
        JSValue args[2] = {string(mContext, path), scalarToJs(mContext, scalar)};
        JSValue result = JS_Call(mContext, writeCallback, JS_UNDEFINED, 2, args);
        for (JSValue arg: args)
            JS_FreeValue(mContext, arg);
        if (JS_IsException(result))
            JS_FreeValue(mContext, JS_GetException(mContext));
        JS_FreeValue(mContext, result);
    };
    callbacks.mOnClose = [this, writeCallback, closeCallback,
                          open](ServerNetworkHandler::DataDrivenScreenOutcome outcome) {
        if (!*open)
            return;
        *open = false;
        JSValue reason = string(mContext, outcomeName(outcome));
        JSValue result = JS_Call(mContext, closeCallback, JS_UNDEFINED, 1, &reason);
        JS_FreeValue(mContext, reason);
        if (JS_IsException(result))
            JS_FreeValue(mContext, JS_GetException(mContext));
        JS_FreeValue(mContext, result);
        JS_FreeValue(mContext, writeCallback);
        JS_FreeValue(mContext, closeCallback);
    };
    const int32_t formId = mHost.showDataDrivenScreen(*player, screenId, propertyPrefix, std::move(storeValue),
                                                      toPathList(mContext, writable), callbacks);
    if (formId == 0) {
        JS_FreeValue(mContext, writeCallback);
        JS_FreeValue(mContext, closeCallback);
    }
    return JS_NewInt32(mContext, formId);
}

JSValue ScriptApi::updateScreen(JSValueConst playerValue, int32_t formId, JSValueConst value, JSValueConst writable) {
    ServerPlayer *player = resolvePlayer(playerValue);
    if (player == nullptr)
        return JS_UNDEFINED;

    DataStoreChangeValue storeValue;
    if (!toStoreValue(mContext, value, storeValue, 0))
        return JS_ThrowTypeError(mContext, "Native type conversion failed.");
    mHost.updateDataDrivenScreen(*player, formId, std::move(storeValue), toPathList(mContext, writable));
    return JS_UNDEFINED;
}

void ScriptApi::closeScreen(JSValueConst playerValue, int32_t formId) {
    ServerPlayer *player = resolvePlayer(playerValue);
    if (player != nullptr)
        mHost.closeDataDrivenScreen(*player, formId);
}

// getFilteredText (sub_14B65D150) through TextFilteringUtils::processMessagesWithClientsideDebugTimingsAsync
// (sub_145A2E9D0, 26.60): without a text processor the text comes back as sent, after the client's debug
// timeout when it set one; a player who turned the filter off gets DisabledByPlayer (error 5, sub_14B6C7190)
JSValue ScriptApi::filterText(JSValueConst playerValue, const std::string &text) {
    ServerPlayer *player = resolvePlayer(playerValue);
    if (player == nullptr)
        return JS_UNDEFINED;

    JSValue resolvingFuncs[2];
    JSValue promise = JS_NewPromiseCapability(mContext, resolvingFuncs);
    JS_FreeValue(mContext, resolvingFuncs[1]);

    PendingFilteredText pending;
    pending.mResolve = resolvingFuncs[0];
    pending.mReadyAt = std::chrono::steady_clock::now();
    if (player->isProfanityFilterEnabled()) {
        pending.mValue = string(mContext, text);
        if (player->getTextFilteringDebugTimeout() > 0)
            pending.mReadyAt += std::chrono::milliseconds(player->getTextFilteringDebugTimeout());
    } else {
        pending.mValue = JS_NewArray(mContext);
        JS_SetPropertyUint32(mContext, pending.mValue, 0, string(mContext, "DisabledByPlayer"));
    }
    mPendingFilteredTexts.push_back(pending);
    return promise;
}

void ScriptApi::_resolveFilteredTexts() {
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    std::vector<PendingFilteredText> ready;
    for (size_t index = 0; index < mPendingFilteredTexts.size();) {
        if (mPendingFilteredTexts[index].mReadyAt > now) {
            ++index;
            continue;
        }
        ready.push_back(mPendingFilteredTexts[index]);
        mPendingFilteredTexts.erase(mPendingFilteredTexts.begin() + (std::ptrdiff_t) index);
    }
    for (PendingFilteredText &pending: ready) {
        JSValue result = JS_Call(mContext, pending.mResolve, JS_UNDEFINED, 1, &pending.mValue);
        JS_FreeValue(mContext, result);
        JS_FreeValue(mContext, pending.mValue);
        JS_FreeValue(mContext, pending.mResolve);
    }
}

void ScriptApi::_resolveForm(uint32_t formId, const std::string &response, const char *status) {
    auto it = mFormResolvers.find(formId);
    if (it == mFormResolvers.end())
        return;

    JSValue resolve = it->second;
    mFormResolvers.erase(it);

    JSValue result = JS_NewObject(mContext);
    JS_SetPropertyStr(mContext, result, "status", JS_NewString(mContext, status));

    JSValue value = JS_NULL;
    if (!response.empty()) {
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

JSValue ScriptApi::makePermutation(const BlockState &state) {
    for (const auto &entry: mServerExports) {
        if (entry.first != "BlockPermutation")
            continue;

        JSValue resolve = JS_GetPropertyStr(mContext, entry.second, "resolve");
        JSValue args[2] = {string(mContext, state.mName), statesToJs(mContext, state.mStates)};
        JSValue permutation = JS_Call(mContext, resolve, entry.second, 2, args);
        for (JSValue arg: args)
            JS_FreeValue(mContext, arg);
        JS_FreeValue(mContext, resolve);
        if (!JS_IsException(permutation))
            return permutation;
        JS_FreeValue(mContext, JS_GetException(mContext));
        break;
    }
    return JS_UNDEFINED;
}

bool ScriptApi::readPermutation(JSValueConst permutation, BlockState &state) {
    if (!JS_IsObject(permutation))
        return false;

    JSValue type = JS_GetPropertyStr(mContext, permutation, "type");
    JSValue id = JS_IsObject(type) ? JS_GetPropertyStr(mContext, type, "id") : JS_UNDEFINED;
    const bool named = JS_IsString(id);
    const std::string identifier = named ? toString(mContext, id) : std::string();
    JS_FreeValue(mContext, id);
    JS_FreeValue(mContext, type);
    if (!named)
        return false;

    JSValue getAll = JS_GetPropertyStr(mContext, permutation, "getAllStates");
    JSValue states = JS_IsFunction(mContext, getAll) ? JS_Call(mContext, getAll, permutation, 0, nullptr)
                                                     : JS_UNDEFINED;
    if (JS_IsException(states)) {
        JS_FreeValue(mContext, JS_GetException(mContext));
        states = JS_UNDEFINED;
    }
    state = BlockState(identifier, statesFromJs(mContext, states));
    JS_FreeValue(mContext, states);
    JS_FreeValue(mContext, getAll);
    return true;
}

BlockState ScriptApi::defaultBlockState(const std::string &identifier) {
    const CustomBlockDefinition *custom = CustomContentRegistry::getInstance().findBlock(identifier);
    if (custom != nullptr)
        return BlockState(identifier, CustomContentRegistry::defaultStatesOf(*custom));
    const Tag *states = BlockPaletteRegistry::getInstance().getDefaultStates(identifier);
    return states == nullptr ? BlockState(identifier) : BlockState(identifier, *states);
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
    if (!hasSubscribers("before.itemUseOn"))
        return;

    const ItemStack &held = player.getInventory().getItemInHand();
    if (held.isAir() || held.mDefinition == nullptr)
        return;

    const std::string typeId = held.mDefinition->getIdentifier();
    JSValue before = makeEvent("WorldBeforeEvents.itemUseOn");
    JS_SetPropertyStr(mContext, before, "source", makePlayer(player));
    JS_SetPropertyStr(mContext, before, "block", makeBlock(mHost.getLevelFor(player), x, y, z));
    JS_SetPropertyStr(mContext, before, "itemStack", makeHeldItemStack(player, typeId));
    JS_SetPropertyStr(mContext, before, "cancel", JS_FALSE);
    emit("before.itemUseOn", before);
}

void ScriptApi::_fireStep(Actor &actor, const Level &level, const std::string &blockId, const Vector3i &position,
                          const char *hook, const char *eventClass) {
    const CustomBlockDefinition *block = CustomContentRegistry::getInstance().findBlock(blockId);
    if (block == nullptr)
        return;
    const BlockState *state = const_cast<Level &>(level).peekBlockPtr(position.x, position.y, position.z);
    const std::vector<CustomComponentUse> uses = CustomContentRegistry::componentsFor(
            *block, state != nullptr && state->mName == blockId ? state->mStates
                                                                : CustomContentRegistry::defaultStatesOf(*block));
    if (!handlesHook(true, uses, hook))
        return;

    JSValue event = makeComponentEvent(eventClass);
    if (JS_IsUndefined(event))
        return;
    fillBlockEvent(event, level, position);
    JS_SetPropertyStr(mContext, event, "entity", makeEntity(actor));
    fireComponents(true, uses, hook, event);
    JS_FreeValue(mContext, event);
}

void ScriptApi::emitBlockStep(Actor &actor, const Level &level, const CustomBlockDefinition *block,
                              const Vector3i &position) {
    const auto previous = mStepping.find(actor.getUniqueId());
    if (previous != mStepping.end() && block != nullptr && previous->second.mIdentifier == block->mIdentifier
        && previous->second.mPosition.x == position.x && previous->second.mPosition.y == position.y
        && previous->second.mPosition.z == position.z)
        return;

    if (previous != mStepping.end()) {
        const SteppedBlock left = previous->second;
        mStepping.erase(previous);
        _fireStep(actor, level, left.mIdentifier, left.mPosition, "onStepOff", "BlockComponentStepOffEvent");
    }

    if (block == nullptr)
        return;
    mStepping[actor.getUniqueId()] = {block->mIdentifier, position};
    _fireStep(actor, level, block->mIdentifier, position, "onStepOn", "BlockComponentStepOnEvent");
}

void ScriptApi::_subscribeGameEvents() {
    EventBus &bus = mHost.getEventBus();

    bus.after().mPlayerBreakBlock.subscribe([this](PlayerBreakBlockAfterEvent &event) {
        Level &level = mHost.getLevelFor(event.mPlayer);
        if (hasSubscribers("after.playerBreakBlock")) {
            JSValue object = makeEvent("WorldAfterEvents.playerBreakBlock");
            JS_SetPropertyStr(mContext, object, "player", makePlayer(event.mPlayer));
            JS_SetPropertyStr(mContext, object, "block",
                              makeBlock(level, event.mBlockPosition.x, event.mBlockPosition.y, event.mBlockPosition.z));
            JS_SetPropertyStr(mContext, object, "dimension", makeDimension(level));
            JS_SetPropertyStr(mContext, object, "brokenBlockPermutation",
                              makePermutation(event.mBrokenBlockIdentifier));
            emit("after.playerBreakBlock", object);
        }

        const ItemStack &held = event.mPlayer.getInventory().getItemInHand();
        const CustomItemDefinition *item = held.isAir() || held.mDefinition == nullptr
                                                   ? nullptr
                                                   : CustomContentRegistry::getInstance().findItem(
                                                             held.mDefinition->getIdentifier());
        if (item != nullptr && handlesHook(false, item->mCustomComponents, "onMineBlock")) {
            JSValue mine = makeComponentEvent("ItemComponentMineBlockEvent");
            if (!JS_IsUndefined(mine)) {
                JS_SetPropertyStr(mContext, mine, "source", makePlayer(event.mPlayer));
                JS_SetPropertyStr(
                        mContext, mine, "block",
                        makeBlock(level, event.mBlockPosition.x, event.mBlockPosition.y, event.mBlockPosition.z));
                JS_SetPropertyStr(mContext, mine, "itemStack", makeItem(held));
                JS_SetPropertyStr(mContext, mine, "minedBlockPermutation",
                                  makePermutation(event.mBrokenBlockIdentifier));
                fireComponents(false, item->mCustomComponents, "onMineBlock", mine);
                JS_FreeValue(mContext, mine);
            }
        }

        const CustomBlockDefinition *block = CustomContentRegistry::getInstance().findBlock(
                event.mBrokenBlockIdentifier);
        if (block == nullptr)
            return;

        // the hook was renamed with the 2.0 module, its permutation field too
        const char *hook = isStable1() ? "onPlayerDestroy" : "onPlayerBreak";
        if (!handlesHook(true, block->mCustomComponents, hook))
            return;
        JSValue broken = makeComponentEvent(isStable1() ? "BlockComponentPlayerDestroyEvent"
                                                        : "BlockComponentPlayerBreakEvent");
        if (JS_IsUndefined(broken))
            return;
        fillBlockEvent(broken, level, event.mBlockPosition);
        JS_SetPropertyStr(mContext, broken, "player", makePlayer(event.mPlayer));
        JS_SetPropertyStr(mContext, broken, isStable1() ? "destroyedBlockPermutation" : "brokenBlockPermutation",
                          makePermutation(event.mBrokenBlockIdentifier));
        fireComponents(true, block->mCustomComponents, hook, broken);
        JS_FreeValue(mContext, broken);
    });

    bus.after().mPlayerPlaceBlock.subscribe([this](PlayerPlaceBlockAfterEvent &event) {
        Level &level = mHost.getLevelFor(event.mPlayer);
        if (hasSubscribers("after.playerPlaceBlock")) {
            JSValue object = makeEvent("WorldAfterEvents.playerPlaceBlock");
            JS_SetPropertyStr(mContext, object, "player", makePlayer(event.mPlayer));
            JS_SetPropertyStr(mContext, object, "block",
                              makeBlock(level, event.mBlockPosition.x, event.mBlockPosition.y, event.mBlockPosition.z));
            JS_SetPropertyStr(mContext, object, "dimension", makeDimension(level));
            emit("after.playerPlaceBlock", object);
        }
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
        JS_SetPropertyStr(mContext, object, "block",
                          makeBlock(level, event.mBlockPosition.x, event.mBlockPosition.y, event.mBlockPosition.z));
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

namespace {
    std::vector<std::string> optionStrings(JSContext *ctx, JSValueConst options, const char *key, bool &present) {
        std::vector<std::string> values;
        JSValue list = JS_GetPropertyStr(ctx, options, key);
        present = JS_IsArray(list);
        if (present) {
            uint32_t length = 0;
            JSValue lengthValue = JS_GetPropertyStr(ctx, list, "length");
            JS_ToUint32(ctx, &length, lengthValue);
            JS_FreeValue(ctx, lengthValue);
            for (uint32_t index = 0; index < length; ++index) {
                JSValue element = JS_GetPropertyUint32(ctx, list, index);
                values.push_back(toString(ctx, element));
                JS_FreeValue(ctx, element);
            }
        }
        JS_FreeValue(ctx, list);
        return values;
    }

    std::string itemName(const ItemStack &item) {
        return item.isAir() || item.mDefinition == nullptr ? std::string() : item.mDefinition->getIdentifier();
    }

    bool namedIn(const std::vector<std::string> &names, const std::string &identifier) {
        for (const std::string &name: names) {
            const std::string full = name.find(':') == std::string::npos ? "minecraft:" + name : name;
            if (!identifier.empty() && full == identifier)
                return true;
        }
        return false;
    }

    bool taggedIn(const std::vector<std::string> &tags, const std::string &identifier) {
        if (identifier.empty())
            return false;
        for (const std::string &tag: CraftingRecipeTable::getItemTags(identifier)) {
            if (std::find(tags.begin(), tags.end(), tag) != tags.end())
                return true;
        }
        return false;
    }

    // InventoryItemEventOptions against one slot change; the item filters look at both stacks
    bool acceptsInventoryChange(JSContext *ctx, JSValueConst options, int32_t slot, const char *inventoryType,
                                const ItemStack &before, const ItemStack &after) {
        bool present = false;
        const std::vector<std::string> slots = optionStrings(ctx, options, "allowedSlots", present);
        if (present && std::find(slots.begin(), slots.end(), std::to_string(slot)) == slots.end())
            return false;

        JSValue type = JS_GetPropertyStr(ctx, options, "inventoryType");
        const bool typeMismatch = JS_IsString(type) && toString(ctx, type) != inventoryType;
        JS_FreeValue(ctx, type);
        if (typeMismatch)
            return false;

        JSValue ignore = JS_GetPropertyStr(ctx, options, "ignoreQuantityChange");
        const bool ignoreQuantity = JS_ToBool(ctx, ignore) == 1;
        JS_FreeValue(ctx, ignore);
        if (ignoreQuantity && !before.isAir() && !after.isAir() && before.mDefinition == after.mDefinition
            && before.mDamage == after.mDamage && before.mTag == after.mTag)
            return false;

        const std::string names[2] = {itemName(before), itemName(after)};
        const std::vector<std::string> includeItems = optionStrings(ctx, options, "includeItems", present);
        if (present && !namedIn(includeItems, names[0]) && !namedIn(includeItems, names[1]))
            return false;
        const std::vector<std::string> includeTags = optionStrings(ctx, options, "includeTags", present);
        if (present && !taggedIn(includeTags, names[0]) && !taggedIn(includeTags, names[1]))
            return false;
        const std::vector<std::string> excludeItems = optionStrings(ctx, options, "excludeItems", present);
        if (present && (namedIn(excludeItems, names[0]) || namedIn(excludeItems, names[1])))
            return false;
        const std::vector<std::string> excludeTags = optionStrings(ctx, options, "excludeTags", present);
        return !(present && (taggedIn(excludeTags, names[0]) || taggedIn(excludeTags, names[1])));
    }
}

// slots 0 to 8 are the hotbar; the same absolute indexes for both types, not checked against the game
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

                const char *type = slot < (size_t) PlayerInventory::HOTBAR_SIZE ? "Hotbar" : "Inventory";
                JSValue event = makeEvent("WorldAfterEvents.playerInventoryItemChange");
                JS_SetPropertyStr(mContext, event, "player", makePlayer(player));
                JS_SetPropertyStr(mContext, event, "slot", JS_NewInt32(mContext, (int32_t) slot));
                JS_SetPropertyStr(mContext, event, "inventoryType", JS_NewString(mContext, type));
                JS_SetPropertyStr(mContext, event, "itemStack", makeItem(after));
                JS_SetPropertyStr(mContext, event, "beforeItemStack", makeItem(before));
                emitFiltered("after.playerInventoryItemChange", event, [&](JSValueConst options) {
                    return acceptsInventoryChange(mContext, options, (int32_t) slot, type, before, after);
                });
            }
        }

        current[key] = contents;
    }

    mInventorySnapshots = std::move(current);
}

// a cursor change between two ticks releases the old stack, then grabs the new one; not checked against the game
void ScriptApi::_emitCursorChanges() {
    const bool grab = hasSubscribers("after.playerCursorItemGrab");
    const bool release = hasSubscribers("after.playerCursorItemRelease");
    if (!grab && !release) {
        mCursorSnapshots.clear();
        return;
    }

    std::unordered_map<uint64_t, ItemStack> current;
    for (auto &entry: mHost.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned())
            continue;
        const ItemStack &cursor = player.getInventory().getCursor();
        const uint64_t key = player.getRuntimeId();
        const auto previous = mCursorSnapshots.find(key);
        if (previous != mCursorSnapshots.end()) {
            const ItemStack &before = previous->second;
            const bool same = before.mDefinition == cursor.mDefinition && before.mCount == cursor.mCount
                              && before.mDamage == cursor.mDamage && before.mTag == cursor.mTag;
            if (!same && !before.isAir() && release) {
                JSValue event = makeEvent("WorldAfterEvents.playerCursorItemRelease");
                JS_SetPropertyStr(mContext, event, "player", makePlayer(player));
                JS_SetPropertyStr(mContext, event, "item", makeItem(before));
                emit("after.playerCursorItemRelease", event);
            }
            if (!same && !cursor.isAir() && grab) {
                JSValue event = makeEvent("WorldAfterEvents.playerCursorItemGrab");
                JS_SetPropertyStr(mContext, event, "player", makePlayer(player));
                JS_SetPropertyStr(mContext, event, "item", makeItem(cursor));
                emit("after.playerCursorItemGrab", event);
            }
        }
        current[key] = cursor;
    }
    mCursorSnapshots = std::move(current);
}
