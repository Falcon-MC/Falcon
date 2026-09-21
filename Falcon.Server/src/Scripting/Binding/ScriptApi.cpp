#include "Scripting/Binding/ScriptApi.h"

#include "Actor/DynamicPropertyStore.h"
#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Command/CommandMap.h"
#include "Command/ServerCommandOrigin.h"
#include "Command/PlayerCommandOrigin.h"
#include "Core/Debug/BedrockLog.h"
#include "Block/BlockPaletteRegistry.h"
#include "Core/Event/EventBus.h"
#include "Inventory/InventoryManager.h"
#include "Item/CraftingRecipeTable.h"
#include "Item/StringToItemParser.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/BlockStateHasher.h"
#include "Protocol/Packets/UpdateBlockPacket.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

namespace {
    std::string toStdString(JSContext *ctx, JSValueConst value) {
        const char *chars = JS_ToCString(ctx, value);
        if (chars == nullptr)
            return std::string();

        std::string result(chars);
        JS_FreeCString(ctx, chars);
        return result;
    }

    JSValue jsString(JSContext *ctx, const std::string &value) {
        return JS_NewStringLen(ctx, value.data(), value.size());
    }

    std::string messageToRawText(JSContext *ctx, JSValueConst value, bool &isJson) {
        if (!JS_IsObject(value)) {
            isJson = false;
            return toStdString(ctx, value);
        }

        isJson = true;
        JSValue rawtext = JS_GetPropertyStr(ctx, value, "rawtext");
        JSValue target;
        if (JS_IsUndefined(rawtext)) {
            JSValue wrap = JS_NewObject(ctx);
            JSValue array = JS_NewArray(ctx);
            JS_SetPropertyUint32(ctx, array, 0, JS_DupValue(ctx, value));
            JS_SetPropertyStr(ctx, wrap, "rawtext", array);
            target = wrap;
        } else {
            target = JS_DupValue(ctx, value);
        }
        JS_FreeValue(ctx, rawtext);

        JSValue json = JS_JSONStringify(ctx, target, JS_UNDEFINED, JS_UNDEFINED);
        std::string result = toStdString(ctx, json);
        JS_FreeValue(ctx, json);
        JS_FreeValue(ctx, target);
        return result;
    }

    JSValue makeVector3(JSContext *ctx, float x, float y, float z) {
        JSValue object = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, object, "x", JS_NewFloat64(ctx, x));
        JS_SetPropertyStr(ctx, object, "y", JS_NewFloat64(ctx, y));
        JS_SetPropertyStr(ctx, object, "z", JS_NewFloat64(ctx, z));
        return object;
    }

    bool readVector3(JSContext *ctx, JSValueConst value, double &x, double &y, double &z) {
        if (!JS_IsObject(value))
            return false;

        JSValue xValue = JS_GetPropertyStr(ctx, value, "x");
        JSValue yValue = JS_GetPropertyStr(ctx, value, "y");
        JSValue zValue = JS_GetPropertyStr(ctx, value, "z");

        const bool ok = JS_ToFloat64(ctx, &x, xValue) == 0 && JS_ToFloat64(ctx, &y, yValue) == 0 &&
                        JS_ToFloat64(ctx, &z, zValue) == 0;

        JS_FreeValue(ctx, xValue);
        JS_FreeValue(ctx, yValue);
        JS_FreeValue(ctx, zValue);
        return ok;
    }

    struct ScriptItem {
        std::string mTypeId;
        int32_t mAmount = 1;
        int32_t mDamage = 0;
        std::unordered_map<std::string, DynamicPropertyValue> mDynamicProperties;
    };

    JSValue dynamicPropertyToJs(JSContext *ctx, const DynamicPropertyValue &value) {
        switch (value.mType) {
            case DynamicPropertyValue::Type::Boolean:
                return JS_NewBool(ctx, value.mBoolean);
            case DynamicPropertyValue::Type::Number:
                return JS_NewFloat64(ctx, value.mNumber);
            case DynamicPropertyValue::Type::String:
                return JS_NewStringLen(ctx, value.mString.data(), value.mString.size());
            case DynamicPropertyValue::Type::Vector: {
                JSValue object = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, object, "x", JS_NewFloat64(ctx, value.mVector[0]));
                JS_SetPropertyStr(ctx, object, "y", JS_NewFloat64(ctx, value.mVector[1]));
                JS_SetPropertyStr(ctx, object, "z", JS_NewFloat64(ctx, value.mVector[2]));
                return object;
            }
            default:
                return JS_UNDEFINED;
        }
    }

    JSValue dynamicPropertyGet(JSContext *ctx, const std::unordered_map<std::string, DynamicPropertyValue> &store,
                               const std::string &id) {
        const auto it = store.find(id);
        if (it == store.end())
            return JS_UNDEFINED;
        return dynamicPropertyToJs(ctx, it->second);
    }

    void dynamicPropertySet(JSContext *ctx, std::unordered_map<std::string, DynamicPropertyValue> &store,
                            const std::string &id, JSValueConst value) {
        if (JS_IsUndefined(value) || JS_IsNull(value)) {
            store.erase(id);
            return;
        }

        DynamicPropertyValue stored;
        if (JS_IsBool(value)) {
            stored.mType = DynamicPropertyValue::Type::Boolean;
            stored.mBoolean = JS_ToBool(ctx, value) == 1;
        } else if (JS_IsNumber(value)) {
            stored.mType = DynamicPropertyValue::Type::Number;
            JS_ToFloat64(ctx, &stored.mNumber, value);
        } else if (JS_IsString(value)) {
            const char *chars = JS_ToCString(ctx, value);
            stored.mType = DynamicPropertyValue::Type::String;
            if (chars != nullptr) {
                stored.mString = chars;
                JS_FreeCString(ctx, chars);
            }
        } else if (JS_IsObject(value)) {
            double x = 0.0, y = 0.0, z = 0.0;
            JSValue xValue = JS_GetPropertyStr(ctx, value, "x");
            JSValue yValue = JS_GetPropertyStr(ctx, value, "y");
            JSValue zValue = JS_GetPropertyStr(ctx, value, "z");
            JS_ToFloat64(ctx, &x, xValue);
            JS_ToFloat64(ctx, &y, yValue);
            JS_ToFloat64(ctx, &z, zValue);
            JS_FreeValue(ctx, xValue);
            JS_FreeValue(ctx, yValue);
            JS_FreeValue(ctx, zValue);
            stored.mType = DynamicPropertyValue::Type::Vector;
            stored.mVector[0] = (float) x;
            stored.mVector[1] = (float) y;
            stored.mVector[2] = (float) z;
        } else {
            return;
        }

        store[id] = stored;
    }

    JSValue dynamicPropertyIds(JSContext *ctx, const std::unordered_map<std::string, DynamicPropertyValue> &store) {
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const auto &entry: store)
            JS_SetPropertyUint32(ctx, array, index++, JS_NewStringLen(ctx, entry.first.data(), entry.first.size()));
        return array;
    }

    bool readItemFromJs(JSContext *ctx, JSClassID itemStackClassId, JSValueConst value, std::string &typeId,
                        int32_t &amount) {
        void *opaque = JS_GetOpaque(value, itemStackClassId);
        if (opaque != nullptr) {
            ScriptItem *item = (ScriptItem *) opaque;
            typeId = item->mTypeId;
            amount = item->mAmount;
            return !typeId.empty();
        }

        if (!JS_IsObject(value))
            return false;

        JSValue typeValue = JS_GetPropertyStr(ctx, value, "typeId");
        JSValue amountValue = JS_GetPropertyStr(ctx, value, "amount");

        const char *chars = JS_ToCString(ctx, typeValue);
        if (chars != nullptr) {
            typeId = chars;
            JS_FreeCString(ctx, chars);
        }

        int32_t parsed = 1;
        if (!JS_IsUndefined(amountValue))
            JS_ToInt32(ctx, &parsed, amountValue);
        amount = parsed;

        JS_FreeValue(ctx, typeValue);
        JS_FreeValue(ctx, amountValue);
        return !typeId.empty();
    }

    JSValue permutationGetState(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return JS_UNDEFINED;
    }

    JSValue makeBlockType(JSContext *ctx, const std::string &identifier) {
        JSValue type = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, type, "id", jsString(ctx, identifier));

        JSValue permutation = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, permutation, "type", type);
        JS_SetPropertyStr(ctx, permutation, "getState",
                          JS_NewCFunction(ctx, permutationGetState, "getState", 1));
        JS_SetPropertyStr(ctx, permutation, "getAllStates",
                          JS_NewCFunction(ctx, permutationGetState, "getAllStates", 0));
        return permutation;
    }

    JSValue makeDamageSource(JSContext *ctx, const std::string &cause) {
        JSValue source = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, source, "cause", jsString(ctx, cause));
        return source;
    }
}

ScriptApi::ScriptApi(JSContext *context, JSRuntime *runtime, ServerNetworkHandler &host)
        : mContext(context), mRuntime(runtime), mHost(host), mScheduler(context), mPlayerClassId(0),
          mPlayerPrototype(JS_UNDEFINED), mActorClassId(0), mActorPrototype(JS_UNDEFINED),
          mBlockClassId(0), mBlockPrototype(JS_UNDEFINED),
          mItemStackClassId(0), mItemStackPrototype(JS_UNDEFINED), mWorld(JS_UNDEFINED), mSystem(JS_UNDEFINED),
          mAfterEvents(JS_UNDEFINED), mBeforeEvents(JS_UNDEFINED), mNextFormId(1) {}

ScriptApi::~ScriptApi() {
    for (int i = 0; i < (int) ScriptEvent::Count; ++i) {
        for (JSValue callback: mSubscribers[i])
            JS_FreeValue(mContext, callback);
        mSubscribers[i].clear();
    }

    for (auto &entry: mNamedSubscribers) {
        for (JSValue callback: entry.second)
            JS_FreeValue(mContext, callback);
    }
    mNamedSubscribers.clear();

    for (auto &entry: mFormResolvers)
        JS_FreeValue(mContext, entry.second);
    mFormResolvers.clear();

    JS_FreeValue(mContext, mBeforeEvents);
    JS_FreeValue(mContext, mAfterEvents);
    JS_FreeValue(mContext, mSystem);
    JS_FreeValue(mContext, mWorld);
    JS_FreeValue(mContext, mItemStackPrototype);
    JS_FreeValue(mContext, mBlockPrototype);
    JS_FreeValue(mContext, mActorPrototype);
    JS_FreeValue(mContext, mPlayerPrototype);
}

ScriptApi *ScriptApi::fromRuntime(JSRuntime *runtime) {
    return (ScriptApi *) JS_GetRuntimeOpaque(runtime);
}

// ========================= Player =========================

ServerPlayer *ScriptApi::resolvePlayer(JSValueConst value) {
    void *opaque = JS_GetOpaque(value, mPlayerClassId);
    const uint32_t handle = (uint32_t) (uintptr_t) opaque;

    const NetworkIdentifier *id = mPlayers.lookup(handle);
    if (id == nullptr)
        return nullptr;

    std::unordered_map<NetworkIdentifier, ServerPlayer, NetworkIdentifier::Hasher> &players = mHost.getPlayers();
    auto it = players.find(*id);
    return it == players.end() ? nullptr : &it->second;
}

JSValue ScriptApi::makePlayer(ServerPlayer &player) {
    const uint32_t handle = mPlayers.acquire(player.getNetworkIdentifier());
    JSValue object = JS_NewObjectProtoClass(mContext, mPlayerPrototype, mPlayerClassId);
    JS_SetOpaque(object, (void *) (uintptr_t) handle);
    return object;
}

ServerPlayer *ScriptApi::resolvePlayerByHandle(uint32_t handle) {
    const NetworkIdentifier *id = mPlayers.lookup(handle);
    if (id == nullptr)
        return nullptr;

    std::unordered_map<NetworkIdentifier, ServerPlayer, NetworkIdentifier::Hasher> &players = mHost.getPlayers();
    auto it = players.find(*id);
    return it == players.end() ? nullptr : &it->second;
}

bool ScriptApi::giveItem(ServerPlayer &player, const std::string &typeId, int32_t amount) {
    Item item;
    if (!StringToItemParser::getInstance().parse(typeId, item))
        return false;

    std::shared_ptr<ItemDefinition> definition = mHost.getItemDefinitions().getDefinition(item.getIdentifier());
    if (definition == nullptr)
        return false;

    ItemStack stack;
    stack.mDefinition = definition;
    stack.mBlockDefinition = mHost.getBlockDefinitions().getDefinition(item.getIdentifier());
    stack.mCount = amount < 1 ? 1 : amount;

    std::vector<int> touchedSlots;
    const int remaining = player.getInventory().addItemPartial(stack, touchedSlots);
    for (const int slot: touchedSlots)
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);

    return remaining < stack.mCount;
}

bool ScriptApi::setBlockType(DimensionType dimension, int32_t x, int32_t y, int32_t z,
                             const std::string &typeId) {
    std::string identifier = typeId;
    if (identifier.find(':') == std::string::npos)
        identifier = "minecraft:" + identifier;

    Level &level = mHost.getDimension(dimension);
    level.setBlockState(x, y, z, BlockState(identifier));

    const BlockState state = level.getBlockState(x, y, z);
    UpdateBlockPacket update;
    update.mBlockPosition = Vector3i(x, y, z);
    update.mRuntimeId = (uint32_t) BlockStateHasher::hash(state.mName, state.mStates);
    update.mFlags = UpdateBlockPacket::Flag::All;
    update.mDataLayer = 0;
    BlockActionHandler::broadcastToViewers(mHost, level,
                                           Vector3f((float) x + 0.5f, (float) y + 0.5f, (float) z + 0.5f), update);
    return true;
}

int32_t ScriptApi::runCommandAsPlayer(ServerPlayer &player, const std::string &commandLine) {
    CommandOriginData origin;
    origin.mOrigin = CommandOriginType::Scripting;
    origin.mPlayerId = (int64_t) player.getRuntimeId();

    PlayerCommandOrigin sender(mHost, player, origin);
    std::string line = commandLine;
    if (!line.empty() && line[0] == '/')
        line.erase(0, 1);

    return mHost.getCommands().dispatch(sender, line) ? 1 : 0;
}

int32_t ScriptApi::runCommandAsConsole(const std::string &commandLine) {
    ServerCommandOrigin sender(&mHost);
    std::string line = commandLine;
    if (!line.empty() && line[0] == '/')
        line.erase(0, 1);

    return mHost.getCommands().dispatch(sender, line) ? 1 : 0;
}

namespace {
    JSValue containerAddItem(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));

        JSValue handleValue = JS_GetPropertyStr(ctx, thisVal, "_handle");
        uint32_t handle = 0;
        JS_ToUint32(ctx, &handle, handleValue);
        JS_FreeValue(ctx, handleValue);

        ServerPlayer *player = api->resolvePlayerByHandle(handle);
        if (player == nullptr)
            return JS_ThrowTypeError(ctx, "Container owner is not valid");

        std::string typeId;
        int32_t amount = 1;
        if (argc < 1 || !readItemFromJs(ctx, api->itemStackClassId(), argv[0], typeId, amount))
            return JS_ThrowTypeError(ctx, "addItem requires an ItemStack");

        api->giveItem(*player, typeId, amount);
        return JS_UNDEFINED;
    }

    ServerPlayer *containerOwner(JSContext *ctx, JSValueConst thisVal) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        JSValue handleValue = JS_GetPropertyStr(ctx, thisVal, "_handle");
        uint32_t handle = 0;
        JS_ToUint32(ctx, &handle, handleValue);
        JS_FreeValue(ctx, handleValue);
        return api->resolvePlayerByHandle(handle);
    }

    JSValue containerGetItem(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = containerOwner(ctx, thisVal);
        if (player == nullptr || argc < 1)
            return JS_UNDEFINED;

        int32_t slot = 0;
        JS_ToInt32(ctx, &slot, argv[0]);
        if (slot < 0 || slot >= PlayerInventory::CONTAINER_SIZE)
            return JS_UNDEFINED;

        const ItemStack &stack = player->getInventory().getItem(slot);
        if (stack.isAir() || stack.mDefinition == nullptr)
            return JS_UNDEFINED;

        JSValue jsItem = api->makeItemStack(stack.mDefinition->getIdentifier(), stack.mCount);
        ScriptItem *item = (ScriptItem *) JS_GetOpaque(jsItem, api->itemStackClassId());
        if (item != nullptr && stack.mTag.isCompound()) {
            const Tag *dynamicProperties = stack.mTag.get("DynamicProperties");
            if (dynamicProperties != nullptr)
                deserializeDynamicProperties(*dynamicProperties, item->mDynamicProperties);
        }
        return jsItem;
    }

    JSValue containerSetItem(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = containerOwner(ctx, thisVal);
        if (player == nullptr || argc < 1)
            return JS_UNDEFINED;

        int32_t slot = 0;
        JS_ToInt32(ctx, &slot, argv[0]);

        if (argc < 2 || JS_IsUndefined(argv[1]) || JS_IsNull(argv[1])) {
            api->host().setContainerSlot(*player, slot, std::string(), 0, Tag::ofCompound());
            return JS_UNDEFINED;
        }

        ScriptItem *item = (ScriptItem *) JS_GetOpaque(argv[1], api->itemStackClassId());
        std::string typeId;
        int32_t amount = 1;
        Tag dynamicProperties = Tag::ofCompound();
        if (item != nullptr) {
            typeId = item->mTypeId;
            amount = item->mAmount;
            dynamicProperties = serializeDynamicProperties(item->mDynamicProperties);
        } else {
            readItemFromJs(ctx, api->itemStackClassId(), argv[1], typeId, amount);
        }

        api->host().setContainerSlot(*player, slot, typeId, amount, dynamicProperties);
        return JS_UNDEFINED;
    }

    JSValue containerEmptySlotsCount(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerPlayer *player = containerOwner(ctx, thisVal);
        if (player == nullptr)
            return JS_NewInt32(ctx, 0);

        int32_t empty = 0;
        for (int slot = 0; slot < PlayerInventory::CONTAINER_SIZE; ++slot) {
            const ItemStack &stack = player->getInventory().getItem(slot);
            if (stack.isAir())
                empty++;
        }
        return JS_NewInt32(ctx, empty);
    }

    JSValue playerGetTypeId(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (api->resolvePlayer(thisVal) == nullptr)
            return JS_UNDEFINED;
        return JS_NewString(ctx, "minecraft:player");
    }

    JSValue playerGetId(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr)
            return JS_UNDEFINED;
        return jsString(ctx, std::to_string(player->getUniqueId()));
    }

    JSValue playerHasComponent(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (api->resolvePlayer(thisVal) == nullptr || argc < 1)
            return JS_NewBool(ctx, false);

        const std::string id = toStdString(ctx, argv[0]);
        const bool has = id == "minecraft:inventory" || id == "inventory" ||
                         id == "minecraft:equippable" || id == "equippable" ||
                         id == "minecraft:health" || id == "health" ||
                         id == "minecraft:player.exhaustion" || id == "minecraft:cursor_inventory";
        return JS_NewBool(ctx, has);
    }

    JSValue playerAddTag(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr || argc < 1)
            return JS_NewBool(ctx, false);
        player->addTag(toStdString(ctx, argv[0]));
        return JS_NewBool(ctx, true);
    }

    JSValue playerRemoveTag(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr || argc < 1)
            return JS_NewBool(ctx, false);
        return JS_NewBool(ctx, player->removeTag(toStdString(ctx, argv[0])));
    }

    JSValue playerHasTag(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr || argc < 1)
            return JS_NewBool(ctx, false);
        return JS_NewBool(ctx, player->hasTag(toStdString(ctx, argv[0])));
    }

    JSValue playerGetTags(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        JSValue array = JS_NewArray(ctx);
        if (player == nullptr)
            return array;
        uint32_t index = 0;
        for (const std::string &tag: player->getTags())
            JS_SetPropertyUint32(ctx, array, index++, jsString(ctx, tag));
        return array;
    }

    JSValue playerGetName(JSContext *ctx, JSValueConst thisVal) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr)
            return JS_ThrowTypeError(ctx, "Player is not valid");

        return JS_NewStringLen(ctx, player->getName().data(), player->getName().size());
    }

    JSValue playerSendMessage(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr)
            return JS_ThrowTypeError(ctx, "Player is not valid");

        if (argc >= 1) {
            bool isJson = false;
            const std::string text = messageToRawText(ctx, argv[0], isJson);
            if (isJson)
                api->host().sendJsonMessage(*player, text);
            else
                player->sendMessage(text);
        }
        return JS_UNDEFINED;
    }

    JSValue playerGetGameMode(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr)
            return JS_ThrowTypeError(ctx, "Player is not valid");

        static const char *names[] = {"survival", "creative", "adventure", "spectator"};
        const int32_t mode = player->getGameType();
        const char *name = (mode >= 0 && mode <= 3) ? names[mode] : "survival";
        return JS_NewString(ctx, name);
    }

    JSValue playerKick(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr)
            return JS_ThrowTypeError(ctx, "Player is not valid");

        const std::string reason = argc >= 1 ? toStdString(ctx, argv[0]) : "Kicked";
        api->host()._disconnect(player->getNetworkIdentifier(), reason);
        return JS_UNDEFINED;
    }

    JSValue playerGetNameMethod(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return playerGetName(ctx, thisVal);
    }

    JSValue playerGetLocation(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr)
            return JS_ThrowTypeError(ctx, "Player is not valid");

        const Vector3f &position = player->getPosition();
        return makeVector3(ctx, position.x, position.y, position.z);
    }

    JSValue playerGetDimension(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr)
            return JS_ThrowTypeError(ctx, "Player is not valid");

        return api->makeDimension(player->getDimension());
    }

    JSValue playerTeleport(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr)
            return JS_ThrowTypeError(ctx, "Player is not valid");

        double x = 0.0, y = 0.0, z = 0.0;
        if (argc < 1 || !readVector3(ctx, argv[0], x, y, z))
            return JS_ThrowTypeError(ctx, "teleport requires a location {x,y,z}");

        player->teleport(api->host(), Vector3f((float) x, (float) y, (float) z));
        return JS_UNDEFINED;
    }

    JSValue playerRunCommand(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr)
            return JS_ThrowTypeError(ctx, "Player is not valid");

        const std::string line = argc >= 1 ? toStdString(ctx, argv[0]) : std::string();
        const int32_t success = api->runCommandAsPlayer(*player, line);

        JSValue result = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, result, "successCount", JS_NewInt32(ctx, success));
        return result;
    }

    JSValue equippableGetEquipment(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        JSValue handleValue = JS_GetPropertyStr(ctx, thisVal, "_handle");
        uint32_t handle = 0;
        JS_ToUint32(ctx, &handle, handleValue);
        JS_FreeValue(ctx, handleValue);

        ServerPlayer *player = api->resolvePlayerByHandle(handle);
        if (player == nullptr || argc < 1)
            return JS_UNDEFINED;

        const std::string slot = toStdString(ctx, argv[0]);
        const PlayerInventory &inventory = player->getInventory();

        const ItemStack *stack = nullptr;
        if (slot == "Head")
            stack = &inventory.getArmor(PlayerInventory::ARMOR_HEAD);
        else if (slot == "Chest")
            stack = &inventory.getArmor(PlayerInventory::ARMOR_TORSO);
        else if (slot == "Legs")
            stack = &inventory.getArmor(PlayerInventory::ARMOR_LEGS);
        else if (slot == "Feet")
            stack = &inventory.getArmor(PlayerInventory::ARMOR_FEET);
        else if (slot == "Offhand")
            stack = &inventory.getOffhand();
        else
            stack = &inventory.getItemInHand();

        if (stack == nullptr || stack->isAir() || stack->mDefinition == nullptr)
            return JS_UNDEFINED;

        return api->makeItemStack(stack->mDefinition->getIdentifier(), stack->mCount);
    }

    JSValue equippableSetEquipment(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        JSValue handleValue = JS_GetPropertyStr(ctx, thisVal, "_handle");
        uint32_t handle = 0;
        JS_ToUint32(ctx, &handle, handleValue);
        JS_FreeValue(ctx, handleValue);

        ServerPlayer *player = api->resolvePlayerByHandle(handle);
        if (player == nullptr || argc < 1)
            return JS_NewBool(ctx, false);

        const std::string slot = toStdString(ctx, argv[0]);
        std::string typeId;
        int32_t amount = 1;
        if (argc >= 2 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1]))
            readItemFromJs(ctx, api->itemStackClassId(), argv[1], typeId, amount);

        api->host().setPlayerEquipment(*player, slot, typeId, amount);
        return JS_NewBool(ctx, true);
    }

    const ItemStack &equipmentStackForSlot(const PlayerInventory &inventory, const std::string &slot) {
        if (slot == "Head")
            return inventory.getArmor(PlayerInventory::ARMOR_HEAD);
        if (slot == "Chest")
            return inventory.getArmor(PlayerInventory::ARMOR_TORSO);
        if (slot == "Legs")
            return inventory.getArmor(PlayerInventory::ARMOR_LEGS);
        if (slot == "Feet")
            return inventory.getArmor(PlayerInventory::ARMOR_FEET);
        if (slot == "Offhand")
            return inventory.getOffhand();
        return inventory.getItemInHand();
    }

    bool equipmentSlotContext(JSContext *ctx, JSValueConst thisVal, ScriptApi *api, ServerPlayer *&player,
                              std::string &slot) {
        JSValue handleValue = JS_GetPropertyStr(ctx, thisVal, "_handle");
        uint32_t handle = 0;
        JS_ToUint32(ctx, &handle, handleValue);
        JS_FreeValue(ctx, handleValue);

        JSValue slotValue = JS_GetPropertyStr(ctx, thisVal, "_slot");
        slot = toStdString(ctx, slotValue);
        JS_FreeValue(ctx, slotValue);

        player = api->resolvePlayerByHandle(handle);
        return player != nullptr;
    }

    JSValue equipmentSlotGetItem(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = nullptr;
        std::string slot;
        if (!equipmentSlotContext(ctx, thisVal, api, player, slot))
            return JS_UNDEFINED;

        const ItemStack &stack = equipmentStackForSlot(player->getInventory(), slot);
        if (stack.isAir() || stack.mDefinition == nullptr)
            return JS_UNDEFINED;

        JSValue jsItem = api->makeItemStack(stack.mDefinition->getIdentifier(), stack.mCount);
        ScriptItem *item = (ScriptItem *) JS_GetOpaque(jsItem, api->itemStackClassId());
        if (item != nullptr) {
            item->mDamage = stack.mDamage;
            if (stack.mTag.isCompound()) {
                const Tag *dynamicProperties = stack.mTag.get("DynamicProperties");
                if (dynamicProperties != nullptr)
                    deserializeDynamicProperties(*dynamicProperties, item->mDynamicProperties);
            }
        }
        return jsItem;
    }

    JSValue equipmentSlotHasItem(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = nullptr;
        std::string slot;
        if (!equipmentSlotContext(ctx, thisVal, api, player, slot))
            return JS_NewBool(ctx, false);

        const ItemStack &stack = equipmentStackForSlot(player->getInventory(), slot);
        return JS_NewBool(ctx, !(stack.isAir() || stack.mDefinition == nullptr));
    }

    JSValue equipmentSlotSetItem(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = nullptr;
        std::string slot;
        if (!equipmentSlotContext(ctx, thisVal, api, player, slot))
            return JS_UNDEFINED;

        std::string typeId;
        int32_t amount = 1;
        int32_t damage = 0;
        Tag dynamicProperties = Tag::ofCompound();
        if (argc >= 1 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
            ScriptItem *item = (ScriptItem *) JS_GetOpaque(argv[0], api->itemStackClassId());
            if (item != nullptr) {
                typeId = item->mTypeId;
                amount = item->mAmount;
                damage = item->mDamage;
                dynamicProperties = serializeDynamicProperties(item->mDynamicProperties);
            } else {
                readItemFromJs(ctx, api->itemStackClassId(), argv[0], typeId, amount);
            }
        }

        api->host().setPlayerEquipment(*player, slot, typeId, amount, damage, dynamicProperties);
        return JS_UNDEFINED;
    }

    JSValue equippableGetEquipmentSlot(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        JSValue handleValue = JS_GetPropertyStr(ctx, thisVal, "_handle");
        uint32_t handle = 0;
        JS_ToUint32(ctx, &handle, handleValue);
        JS_FreeValue(ctx, handleValue);

        const std::string slot = argc >= 1 ? toStdString(ctx, argv[0]) : std::string("Mainhand");

        JSValue slotObject = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, slotObject, "_handle", JS_NewUint32(ctx, handle));
        JS_SetPropertyStr(ctx, slotObject, "_slot", JS_NewString(ctx, slot.c_str()));
        JS_SetPropertyStr(ctx, slotObject, "getItem",
                          JS_NewCFunction(ctx, equipmentSlotGetItem, "getItem", 0));
        JS_SetPropertyStr(ctx, slotObject, "hasItem",
                          JS_NewCFunction(ctx, equipmentSlotHasItem, "hasItem", 0));
        JS_SetPropertyStr(ctx, slotObject, "setItem",
                          JS_NewCFunction(ctx, equipmentSlotSetItem, "setItem", 1));
        return slotObject;
    }

    JSValue playerGetComponent(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr)
            return JS_ThrowTypeError(ctx, "Player is not valid");

        const std::string id = argc >= 1 ? toStdString(ctx, argv[0]) : std::string();

        if (id == "minecraft:equippable" || id == "equippable") {
            const uint32_t handle = (uint32_t) (uintptr_t) JS_GetOpaque(thisVal, api->playerClassId());
            JSValue component = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, component, "_handle", JS_NewUint32(ctx, handle));
            JS_SetPropertyStr(ctx, component, "getEquipment",
                              JS_NewCFunction(ctx, equippableGetEquipment, "getEquipment", 1));
            JS_SetPropertyStr(ctx, component, "setEquipment",
                              JS_NewCFunction(ctx, equippableSetEquipment, "setEquipment", 2));
            JS_SetPropertyStr(ctx, component, "getEquipmentSlot",
                              JS_NewCFunction(ctx, equippableGetEquipmentSlot, "getEquipmentSlot", 1));
            return component;
        }

        if (id != "minecraft:inventory" && id != "inventory")
            return JS_UNDEFINED;

        const uint32_t handle = (uint32_t) (uintptr_t) JS_GetOpaque(thisVal, api->playerClassId());

        JSValue container = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, container, "_handle", JS_NewUint32(ctx, handle));
        JS_SetPropertyStr(ctx, container, "size", JS_NewInt32(ctx, 36));
        JS_SetPropertyStr(ctx, container, "addItem",
                          JS_NewCFunction(ctx, containerAddItem, "addItem", 1));
        JS_SetPropertyStr(ctx, container, "getItem",
                          JS_NewCFunction(ctx, containerGetItem, "getItem", 1));
        JS_SetPropertyStr(ctx, container, "setItem",
                          JS_NewCFunction(ctx, containerSetItem, "setItem", 2));
        JS_SetPropertyStr(ctx, container, "emptySlotsCount",
                          JS_NewCFunction(ctx, containerEmptySlotsCount, "emptySlotsCount", 0));

        JSValue component = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, component, "typeId", JS_NewString(ctx, "minecraft:inventory"));
        JS_SetPropertyStr(ctx, component, "container", container);
        return component;
    }

    JSValue playerGetDynamicProperty(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr || argc < 1)
            return JS_UNDEFINED;
        return dynamicPropertyGet(ctx, player->getDynamicProperties(), toStdString(ctx, argv[0]));
    }

    JSValue playerSetDynamicProperty(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr || argc < 1)
            return JS_UNDEFINED;
        dynamicPropertySet(ctx, player->getDynamicProperties(), toStdString(ctx, argv[0]),
                           argc >= 2 ? argv[1] : JS_UNDEFINED);
        return JS_UNDEFINED;
    }

    JSValue playerGetDynamicPropertyIds(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr)
            return JS_NewArray(ctx);
        return dynamicPropertyIds(ctx, player->getDynamicProperties());
    }

    JSValue playerClearDynamicProperties(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player != nullptr)
            player->getDynamicProperties().clear();
        return JS_UNDEFINED;
    }

    JSValue playerGetViewDirection(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr)
            return JS_ThrowTypeError(ctx, "Player is not valid");

        const float pitch = player->getRotation().x * 0.017453292f;
        const float yaw = player->getRotation().y * 0.017453292f;
        const float x = -std::sin(yaw) * std::cos(pitch);
        const float y = -std::sin(pitch);
        const float z = std::cos(yaw) * std::cos(pitch);
        return makeVector3(ctx, x, y, z);
    }

    JSValue playerGetHeadLocation(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr)
            return JS_ThrowTypeError(ctx, "Player is not valid");
        const Vector3f &position = player->getPosition();
        return makeVector3(ctx, position.x, position.y + 1.62f, position.z);
    }

    JSValue playerGetRotationMethod(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr)
            return JS_ThrowTypeError(ctx, "Player is not valid");
        JSValue object = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, object, "x", JS_NewFloat64(ctx, player->getRotation().x));
        JS_SetPropertyStr(ctx, object, "y", JS_NewFloat64(ctx, player->getRotation().y));
        return object;
    }

    JSValue playerGetIsSneaking(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr)
            return JS_NewBool(ctx, false);
        return JS_NewBool(ctx, player->getFlags().get(ActorFlag::Sneaking));
    }

    JSValue playerGetIsSprinting(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr)
            return JS_NewBool(ctx, false);
        return JS_NewBool(ctx, player->getFlags().get(ActorFlag::Sprinting));
    }

    JSValue playerGetSelectedSlot(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr)
            return JS_NewInt32(ctx, 0);
        return JS_NewInt32(ctx, player->getInventory().getSelectedSlot());
    }

    JSValue playerPlayAnimationMethod(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player != nullptr && argc >= 1)
            api->host().playPlayerAnimation(*player, toStdString(ctx, argv[0]));
        return JS_UNDEFINED;
    }

    JSValue playerGetItemCooldown(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_NewInt32(ctx, 0);

        const std::string category = toStdString(ctx, argv[0]);
        for (const CustomItemDefinition &definition: CustomContentRegistry::getInstance().getItems()) {
            if (!definition.mCooldownCategory.empty() && definition.mCooldownCategory == category)
                return JS_NewInt32(ctx, definition.mCooldownTicks);
        }
        return JS_NewInt32(ctx, 0);
    }

    JSValue onScreenSetActionBar(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        JSValue handleValue = JS_GetPropertyStr(ctx, thisVal, "_handle");
        uint32_t handle = 0;
        JS_ToUint32(ctx, &handle, handleValue);
        JS_FreeValue(ctx, handleValue);

        ServerPlayer *player = api->resolvePlayerByHandle(handle);
        if (player != nullptr && argc >= 1) {
            bool isJson = false;
            const std::string text = messageToRawText(ctx, argv[0], isJson);
            api->host().sendActionBar(*player, text, isJson);
        }
        return JS_UNDEFINED;
    }

    JSValue onScreenSetTitle(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        JSValue handleValue = JS_GetPropertyStr(ctx, thisVal, "_handle");
        uint32_t handle = 0;
        JS_ToUint32(ctx, &handle, handleValue);
        JS_FreeValue(ctx, handleValue);

        ServerPlayer *player = api->resolvePlayerByHandle(handle);
        if (player != nullptr && argc >= 1) {
            bool isJson = false;
            const std::string text = messageToRawText(ctx, argv[0], isJson);
            api->host().sendTitle(*player, text, isJson);
        }
        return JS_UNDEFINED;
    }

    JSValue noop(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return JS_UNDEFINED;
    }

    JSValue playerGetOnScreenDisplay(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (api->resolvePlayer(thisVal) == nullptr)
            return JS_UNDEFINED;

        const uint32_t handle = (uint32_t) (uintptr_t) JS_GetOpaque(thisVal, api->playerClassId());
        JSValue display = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, display, "_handle", JS_NewUint32(ctx, handle));
        JS_SetPropertyStr(ctx, display, "setActionBar",
                          JS_NewCFunction(ctx, onScreenSetActionBar, "setActionBar", 1));
        JS_SetPropertyStr(ctx, display, "setTitle",
                          JS_NewCFunction(ctx, onScreenSetTitle, "setTitle", 1));
        JS_SetPropertyStr(ctx, display, "updateSubtitle", JS_NewCFunction(ctx, noop, "updateSubtitle", 1));
        JS_SetPropertyStr(ctx, display, "hideAllForms", JS_NewCFunction(ctx, noop, "hideAllForms", 0));
        return display;
    }

    JSValue playerGetInputPermissions(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        JSValue object = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, object, "setPermissionCategory",
                          JS_NewCFunction(ctx, noop, "setPermissionCategory", 2));
        JS_SetPropertyStr(ctx, object, "isPermissionCategoryEnabled",
                          JS_NewCFunction(ctx, [](JSContext *c, JSValueConst, int, JSValueConst *) {
                              return JS_NewBool(c, true);
                          }, "isPermissionCategoryEnabled", 1));
        return object;
    }

    JSValue playerGetLocatorBar(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        JSValue object = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, object, "addWaypoint", JS_NewCFunction(ctx, noop, "addWaypoint", 1));
        JS_SetPropertyStr(ctx, object, "removeWaypoint", JS_NewCFunction(ctx, noop, "removeWaypoint", 1));
        JS_SetPropertyStr(ctx, object, "removeAllWaypoints", JS_NewCFunction(ctx, noop, "removeAllWaypoints", 0));
        return object;
    }

    JSValue playerSetGameMode(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr || argc < 1)
            return JS_UNDEFINED;

        const std::string mode = toStdString(ctx, argv[0]);
        int32_t gameType = 0;
        if (mode == "creative")
            gameType = 1;
        else if (mode == "adventure")
            gameType = 2;
        else if (mode == "spectator")
            gameType = 6;
        api->host().setPlayerGameMode(*player, gameType);
        return JS_UNDEFINED;
    }

    JSValue playerPlaySound(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr || argc < 1)
            return JS_UNDEFINED;

        const std::string sound = toStdString(ctx, argv[0]);
        Vector3f position = player->getPosition();
        float volume = 1.0f;
        float pitch = 1.0f;
        if (argc >= 2 && JS_IsObject(argv[1])) {
            JSValue locationValue = JS_GetPropertyStr(ctx, argv[1], "location");
            double x = 0.0, y = 0.0, z = 0.0;
            if (JS_IsObject(locationValue) && readVector3(ctx, locationValue, x, y, z))
                position = Vector3f((float) x, (float) y, (float) z);
            JS_FreeValue(ctx, locationValue);

            JSValue volumeValue = JS_GetPropertyStr(ctx, argv[1], "volume");
            JSValue pitchValue = JS_GetPropertyStr(ctx, argv[1], "pitch");
            double parsed = 1.0;
            if (JS_ToFloat64(ctx, &parsed, volumeValue) == 0)
                volume = (float) parsed;
            if (JS_ToFloat64(ctx, &parsed, pitchValue) == 0)
                pitch = (float) parsed;
            JS_FreeValue(ctx, volumeValue);
            JS_FreeValue(ctx, pitchValue);
        }

        api->host().playSoundFor(*player, sound, position, volume, pitch);
        return JS_UNDEFINED;
    }

    JSValue playerSpawnParticle(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr || argc < 2)
            return JS_UNDEFINED;

        const std::string identifier = toStdString(ctx, argv[0]);
        double x = 0.0, y = 0.0, z = 0.0;
        if (!readVector3(ctx, argv[1], x, y, z))
            return JS_UNDEFINED;

        api->host().spawnParticleEffect(api->host().getLevelFor(*player), identifier,
                                        Vector3f((float) x, (float) y, (float) z));
        return JS_UNDEFINED;
    }

    JSValue playerStartItemCooldown(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr || argc < 2)
            return JS_UNDEFINED;

        const std::string category = toStdString(ctx, argv[0]);
        int32_t duration = 0;
        JS_ToInt32(ctx, &duration, argv[1]);
        api->host().startPlayerItemCooldown(*player, category, duration);
        return JS_UNDEFINED;
    }

    JSValue playerAddExperience(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr || argc < 1)
            return JS_NewInt32(ctx, 0);

        int32_t amount = 0;
        JS_ToInt32(ctx, &amount, argv[0]);
        player->getExperience().addXp(amount);
        api->host().syncPlayerAttributes(*player);
        return JS_NewInt32(ctx, player->getExperience().getLifetimeTotalXp());
    }

    JSValue playerAddLevels(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr || argc < 1)
            return JS_NewInt32(ctx, 0);

        int32_t levels = 0;
        JS_ToInt32(ctx, &levels, argv[0]);
        player->getExperience().addXpLevels(levels);
        api->host().syncPlayerAttributes(*player);
        return JS_NewInt32(ctx, player->getExperience().getXpLevel());
    }

    JSValue playerResetLevel(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr)
            return JS_UNDEFINED;
        player->getExperience().reset();
        api->host().syncPlayerAttributes(*player);
        return JS_UNDEFINED;
    }

    JSValue playerGetLevel(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerPlayer *player = api->resolvePlayer(thisVal);
        if (player == nullptr)
            return JS_NewInt32(ctx, 0);
        return JS_NewInt32(ctx, player->getExperience().getXpLevel());
    }

    JSValue cameraClear(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        JSValue handleValue = JS_GetPropertyStr(ctx, thisVal, "_handle");
        uint32_t handle = 0;
        JS_ToUint32(ctx, &handle, handleValue);
        JS_FreeValue(ctx, handleValue);

        ServerPlayer *player = api->resolvePlayerByHandle(handle);
        if (player != nullptr)
            api->host().clearPlayerCamera(*player);
        return JS_UNDEFINED;
    }

    JSValue playerGetCamera(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (api->resolvePlayer(thisVal) == nullptr)
            return JS_UNDEFINED;

        const uint32_t handle = (uint32_t) (uintptr_t) JS_GetOpaque(thisVal, api->playerClassId());
        JSValue camera = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, camera, "_handle", JS_NewUint32(ctx, handle));
        JS_SetPropertyStr(ctx, camera, "clear", JS_NewCFunction(ctx, cameraClear, "clear", 0));
        JS_SetPropertyStr(ctx, camera, "setCamera", JS_NewCFunction(ctx, noop, "setCamera", 2));
        JS_SetPropertyStr(ctx, camera, "fade", JS_NewCFunction(ctx, noop, "fade", 1));
        return camera;
    }
}

// ========================= System =========================

namespace {
    JSValue systemRun(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (argc < 1 || !JS_IsFunction(ctx, argv[0]))
            return JS_ThrowTypeError(ctx, "system callback must be a function");

        JSValue callback = JS_DupValue(ctx, argv[0]);
        void *pointer = JS_VALUE_GET_PTR(callback);

        int64_t ticks = 1;
        if (magic != 0 && argc >= 2)
            JS_ToInt64(ctx, &ticks, argv[1]);

        uint32_t id;
        if (magic == 0)
            id = api->scheduler().run(pointer);
        else if (magic == 1)
            id = api->scheduler().runTimeout(pointer, ticks);
        else
            id = api->scheduler().runInterval(pointer, ticks);

        return JS_NewUint32(ctx, id);
    }

    JSValue systemClearRun(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (argc >= 1) {
            uint32_t id = 0;
            JS_ToUint32(ctx, &id, argv[0]);
            api->scheduler().clear(id);
        }
        return JS_UNDEFINED;
    }

    JSValue systemGetCurrentTick(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        return JS_NewInt64(ctx, api->host().getCurrentTick());
    }
}

void ScriptApi::_buildSystem() {
    mSystem = JS_NewObject(mContext);

    JS_SetPropertyStr(mContext, mSystem, "run",
                      JS_NewCFunctionMagic(mContext, systemRun, "run", 1, JS_CFUNC_generic_magic, 0));
    JS_SetPropertyStr(mContext, mSystem, "runTimeout",
                      JS_NewCFunctionMagic(mContext, systemRun, "runTimeout", 2, JS_CFUNC_generic_magic, 1));
    JS_SetPropertyStr(mContext, mSystem, "runInterval",
                      JS_NewCFunctionMagic(mContext, systemRun, "runInterval", 2, JS_CFUNC_generic_magic, 2));
    JS_SetPropertyStr(mContext, mSystem, "clearRun",
                      JS_NewCFunction(mContext, systemClearRun, "clearRun", 1));

    JSAtom tickAtom = JS_NewAtom(mContext, "currentTick");
    JSValue tickGetter = JS_NewCFunction(mContext, systemGetCurrentTick, "get currentTick", 0);
    JS_DefinePropertyGetSet(mContext, mSystem, tickAtom, tickGetter, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
    JS_FreeAtom(mContext, tickAtom);
}

// ========================= World =========================

namespace {
    JSValue worldSendMessage(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (argc < 1)
            return JS_UNDEFINED;

        bool isJson = false;
        const std::string message = messageToRawText(ctx, argv[0], isJson);
        for (auto &entry: api->host().getPlayers()) {
            if (!entry.second.isSpawned())
                continue;
            if (isJson)
                api->host().sendJsonMessage(entry.second, message);
            else
                entry.second.sendMessage(message);
        }
        return JS_UNDEFINED;
    }

    JSValue worldGetAllPlayers(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));

        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (auto &entry: api->host().getPlayers()) {
            if (!entry.second.isSpawned())
                continue;

            JS_SetPropertyUint32(ctx, array, index, api->makePlayer(entry.second));
            index++;
        }
        return array;
    }

    JSValue worldGetDimension(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (argc < 1)
            return api->makeDimension(DimensionType::Overworld);

        std::string identifier = toStdString(ctx, argv[0]);
        if (identifier.find(':') == std::string::npos)
            identifier = "minecraft:" + identifier;

        if (identifier == "minecraft:nether")
            return api->makeDimension(DimensionType::Nether);
        if (identifier == "minecraft:the_end")
            return api->makeDimension(DimensionType::TheEnd);
        if (identifier == "minecraft:overworld")
            return api->makeDimension(DimensionType::Overworld);

        return JS_ThrowTypeError(ctx, "Unknown dimension %s", identifier.c_str());
    }

    JSValue worldGetDynamicProperty(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (argc < 1)
            return JS_UNDEFINED;
        return dynamicPropertyGet(ctx, api->host().getWorldDynamicProperties(), toStdString(ctx, argv[0]));
    }

    JSValue worldSetDynamicProperty(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (argc < 1)
            return JS_UNDEFINED;
        dynamicPropertySet(ctx, api->host().getWorldDynamicProperties(), toStdString(ctx, argv[0]),
                           argc >= 2 ? argv[1] : JS_UNDEFINED);
        return JS_UNDEFINED;
    }

    JSValue worldGetDynamicPropertyIds(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        return dynamicPropertyIds(ctx, api->host().getWorldDynamicProperties());
    }

    JSValue worldClearDynamicProperties(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        api->host().getWorldDynamicProperties().clear();
        return JS_UNDEFINED;
    }

    JSValue worldGetEntity(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (argc < 1)
            return JS_UNDEFINED;

        const std::string idText = toStdString(ctx, argv[0]);
        char *end = nullptr;
        const int64_t uniqueId = (int64_t) strtoll(idText.c_str(), &end, 10);
        if (end == idText.c_str())
            return JS_UNDEFINED;

        ServerActor *actor = api->host().getActor(uniqueId);
        if (actor == nullptr || !actor->isAlive())
            return JS_UNDEFINED;
        return api->makeActor(*actor);
    }
}

void ScriptApi::_buildWorld() {
    mWorld = JS_NewObject(mContext);

    JS_SetPropertyStr(mContext, mWorld, "sendMessage",
                      JS_NewCFunction(mContext, worldSendMessage, "sendMessage", 1));
    JS_SetPropertyStr(mContext, mWorld, "getAllPlayers",
                      JS_NewCFunction(mContext, worldGetAllPlayers, "getAllPlayers", 0));
    JS_SetPropertyStr(mContext, mWorld, "getDimension",
                      JS_NewCFunction(mContext, worldGetDimension, "getDimension", 1));
    JS_SetPropertyStr(mContext, mWorld, "getEntity",
                      JS_NewCFunction(mContext, worldGetEntity, "getEntity", 1));
    JS_SetPropertyStr(mContext, mWorld, "getDynamicProperty",
                      JS_NewCFunction(mContext, worldGetDynamicProperty, "getDynamicProperty", 1));
    JS_SetPropertyStr(mContext, mWorld, "setDynamicProperty",
                      JS_NewCFunction(mContext, worldSetDynamicProperty, "setDynamicProperty", 2));
    JS_SetPropertyStr(mContext, mWorld, "getDynamicPropertyIds",
                      JS_NewCFunction(mContext, worldGetDynamicPropertyIds, "getDynamicPropertyIds", 0));
    JS_SetPropertyStr(mContext, mWorld, "clearDynamicProperties",
                      JS_NewCFunction(mContext, worldClearDynamicProperties, "clearDynamicProperties", 0));

    JS_SetPropertyStr(mContext, mWorld, "afterEvents", JS_DupValue(mContext, mAfterEvents));
    JS_SetPropertyStr(mContext, mWorld, "beforeEvents", JS_DupValue(mContext, mBeforeEvents));
}

// ========================= Events =========================

namespace {
    JSValue eventSubscribe(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (argc < 1 || !JS_IsFunction(ctx, argv[0]))
            return JS_ThrowTypeError(ctx, "event callback must be a function");

        api->addSubscriber((ScriptEvent) magic, JS_DupValue(ctx, argv[0]));
        return JS_DupValue(ctx, argv[0]);
    }

    JSValue eventUnsubscribe(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (argc >= 1)
            api->removeSubscriber((ScriptEvent) magic, argv[0]);
        return JS_UNDEFINED;
    }

    JSValue makeSignal(JSContext *ctx, ScriptEvent event) {
        JSValue signal = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, signal, "subscribe",
                          JS_NewCFunctionMagic(ctx, eventSubscribe, "subscribe", 1, JS_CFUNC_generic_magic,
                                               (int) event));
        JS_SetPropertyStr(ctx, signal, "unsubscribe",
                          JS_NewCFunctionMagic(ctx, eventUnsubscribe, "unsubscribe", 1, JS_CFUNC_generic_magic,
                                               (int) event));
        return signal;
    }

    JSValue namedSubscribe(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (argc < 1 || !JS_IsFunction(ctx, argv[0]))
            return JS_ThrowTypeError(ctx, "event callback must be a function");

        JSValue nameValue = JS_GetPropertyStr(ctx, thisVal, "_name");
        api->addNamedSubscriber(toStdString(ctx, nameValue), JS_DupValue(ctx, argv[0]));
        JS_FreeValue(ctx, nameValue);
        return JS_DupValue(ctx, argv[0]);
    }

    JSValue namedUnsubscribe(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (argc < 1)
            return JS_UNDEFINED;

        JSValue nameValue = JS_GetPropertyStr(ctx, thisVal, "_name");
        api->removeNamedSubscriber(toStdString(ctx, nameValue), argv[0]);
        JS_FreeValue(ctx, nameValue);
        return JS_UNDEFINED;
    }

    JSValue makeNamedSignal(JSContext *ctx, const char *name) {
        JSValue signal = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, signal, "_name", JS_NewString(ctx, name));
        JS_SetPropertyStr(ctx, signal, "subscribe",
                          JS_NewCFunction(ctx, namedSubscribe, "subscribe", 1));
        JS_SetPropertyStr(ctx, signal, "unsubscribe",
                          JS_NewCFunction(ctx, namedUnsubscribe, "unsubscribe", 1));
        return signal;
    }
}

void ScriptApi::_buildEvents() {
    mAfterEvents = JS_NewObject(mContext);
    JS_SetPropertyStr(mContext, mAfterEvents, "playerBreakBlock",
                      makeSignal(mContext, ScriptEvent::AfterPlayerBreakBlock));
    JS_SetPropertyStr(mContext, mAfterEvents, "playerPlaceBlock",
                      makeSignal(mContext, ScriptEvent::AfterPlayerPlaceBlock));
    JS_SetPropertyStr(mContext, mAfterEvents, "playerSpawn",
                      makeSignal(mContext, ScriptEvent::AfterPlayerSpawn));
    JS_SetPropertyStr(mContext, mAfterEvents, "playerJoin",
                      makeSignal(mContext, ScriptEvent::AfterPlayerJoin));
    JS_SetPropertyStr(mContext, mAfterEvents, "playerLeave",
                      makeSignal(mContext, ScriptEvent::AfterPlayerLeave));
    JS_SetPropertyStr(mContext, mAfterEvents, "chatSend",
                      makeSignal(mContext, ScriptEvent::AfterChatSend));
    JS_SetPropertyStr(mContext, mAfterEvents, "entityHurt",
                      makeSignal(mContext, ScriptEvent::AfterEntityHurt));
    JS_SetPropertyStr(mContext, mAfterEvents, "entityDie",
                      makeSignal(mContext, ScriptEvent::AfterEntityDie));
    JS_SetPropertyStr(mContext, mAfterEvents, "itemUse",
                      makeSignal(mContext, ScriptEvent::AfterItemUse));

    static const char *afterNamed[] = {
            "projectileHitBlock", "projectileHitEntity", "entityHitBlock", "entityHitEntity",
            "entityItemDrop", "playerHotbarSelectedSlotChange", "playerInventoryItemChange",
            "playerInteractWithBlock", "entitySpawn", "entityRemove", "entityLoad", "entityDie", "worldLoad"
    };
    for (const char *name: afterNamed)
        JS_SetPropertyStr(mContext, mAfterEvents, name, makeNamedSignal(mContext, name));

    mBeforeEvents = JS_NewObject(mContext);
    JS_SetPropertyStr(mContext, mBeforeEvents, "playerBreakBlock",
                      makeSignal(mContext, ScriptEvent::BeforePlayerBreakBlock));
    JS_SetPropertyStr(mContext, mBeforeEvents, "chatSend",
                      makeSignal(mContext, ScriptEvent::BeforeChatSend));

    static const char *beforeNamed[] = {
            "playerInteractWithBlock", "entityHurt", "effectAdd", "playerInteractWithEntity",
            "itemUse", "chatSend", "worldInitialize", "itemUseOn"
    };
    for (const char *name: beforeNamed)
        JS_SetPropertyStr(mContext, mBeforeEvents, name, makeNamedSignal(mContext, name));
}

void ScriptApi::addSubscriber(ScriptEvent event, JSValue callback) {
    mSubscribers[(int) event].push_back(callback);
}

void ScriptApi::removeSubscriber(ScriptEvent event, JSValueConst callback) {
    std::vector<JSValue> &list = mSubscribers[(int) event];
    for (auto it = list.begin(); it != list.end(); ++it) {
        if (JS_VALUE_GET_PTR(*it) == JS_VALUE_GET_PTR(callback)) {
            JS_FreeValue(mContext, *it);
            list.erase(it);
            return;
        }
    }
}

void ScriptApi::emitProjectileHitBlock(ServerActor &projectile, int32_t x, int32_t y, int32_t z) {
    if (!hasNamedSubscribers("projectileHitBlock"))
        return;

    JSValue event = JS_NewObject(mContext);
    JS_SetPropertyStr(mContext, event, "projectile", makeActor(projectile));

    if (projectile.hasOwnerPlayer()) {
        ServerPlayer *owner = resolvePlayerByHandle(projectile.getOwnerPlayerHandle());
        if (owner != nullptr)
            JS_SetPropertyStr(mContext, event, "source", makePlayer(*owner));
    } else if (projectile.getOwnerUniqueId() >= 0) {
        ServerActor *owner = mHost.getActor(projectile.getOwnerUniqueId());
        if (owner != nullptr)
            JS_SetPropertyStr(mContext, event, "source", makeActor(*owner));
    }

    JSValue block = makeBlock(projectile.getDimension(), x, y, z);
    JSValue hitResult = JS_NewObject(mContext);
    JS_SetPropertyStr(mContext, hitResult, "block", block);
    JS_SetPropertyStr(mContext, hitResult, "_hasBlock", JS_NewBool(mContext, true));

    JSValue location = makeVector3(mContext, (float) x + 0.5f, (float) y + 0.5f, (float) z + 0.5f);
    JS_SetPropertyStr(mContext, hitResult, "faceLocation", location);

    const Vector3f &motion = projectile.getMotion();
    const float absX = std::fabs(motion.x);
    const float absY = std::fabs(motion.y);
    const float absZ = std::fabs(motion.z);
    const char *face = "Up";
    if (absY >= absX && absY >= absZ)
        face = motion.y < 0.0f ? "Up" : "Down";
    else if (absX >= absZ)
        face = motion.x > 0.0f ? "West" : "East";
    else
        face = motion.z > 0.0f ? "North" : "South";
    JS_SetPropertyStr(mContext, hitResult, "face", JS_NewString(mContext, face));

    JS_SetPropertyStr(mContext, event, "_hit", hitResult);

    JSValue getBlockHit = JS_NewCFunction(mContext, [](JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_GetPropertyStr(ctx, thisVal, "_hit");
    }, "getBlockHit", 0);
    JS_SetPropertyStr(mContext, event, "getBlockHit", getBlockHit);

    emitNamed("projectileHitBlock", event);
}

void ScriptApi::fireHeldItemComponents(ServerPlayer &player, const char *hook, JSValue event) {
    const ItemStack &held = player.getInventory().getItemInHand();
    const std::string typeId = (held.isAir() || held.mDefinition == nullptr) ? std::string()
                                                                             : held.mDefinition->getIdentifier();

    const std::vector<std::string> *components = nullptr;
    if (!typeId.empty()) {
        for (const CustomItemDefinition &definition: CustomContentRegistry::getInstance().getItems()) {
            if (definition.mIdentifier == typeId) {
                components = &definition.mCustomComponents;
                break;
            }
        }
    }

    if (components == nullptr || components->empty()) {
        JS_FreeValue(mContext, event);
        return;
    }

    JSValue ids = JS_NewArray(mContext);
    uint32_t index = 0;
    for (const std::string &id: *components)
        JS_SetPropertyUint32(mContext, ids, index++, jsString(mContext, id));

    JSValue global = JS_GetGlobalObject(mContext);
    JSValue fn = JS_GetPropertyStr(mContext, global, "__falconFireItemComponents");
    if (JS_IsFunction(mContext, fn)) {
        JSValue hookValue = jsString(mContext, hook);
        JSValue args[3] = {hookValue, ids, event};
        JSValue result = JS_Call(mContext, fn, JS_UNDEFINED, 3, args);
        JS_FreeValue(mContext, result);
        JS_FreeValue(mContext, hookValue);
    }
    JS_FreeValue(mContext, fn);
    JS_FreeValue(mContext, global);
    JS_FreeValue(mContext, ids);
    JS_FreeValue(mContext, event);
}

void ScriptApi::emitItemUseOnBlock(ServerPlayer &player, int32_t x, int32_t y, int32_t z) {
    const ItemStack &held = player.getInventory().getItemInHand();
    if (held.isAir() || held.mDefinition == nullptr)
        return;

    const std::string typeId = held.mDefinition->getIdentifier();

    JSValue event = JS_NewObject(mContext);
    JS_SetPropertyStr(mContext, event, "source", makePlayer(player));
    JS_SetPropertyStr(mContext, event, "block", makeBlock(player.getDimension(), x, y, z));
    JS_SetPropertyStr(mContext, event, "itemStack", makeHeldItemStack(player, typeId));

    if (hasNamedSubscribers("itemUseOn"))
        emitNamed("itemUseOn", JS_DupValue(mContext, event));

    fireHeldItemComponents(player, "onUseOn", event);
}

void ScriptApi::emitWorldInitialize() {
    if (!hasNamedSubscribers("worldInitialize"))
        return;

    JSValue global = JS_GetGlobalObject(mContext);
    JSValue itemRegistry = JS_GetPropertyStr(mContext, global, "__falconItemComponentRegistry");
    JSValue blockRegistry = JS_GetPropertyStr(mContext, global, "__falconBlockComponentRegistry");
    JS_FreeValue(mContext, global);

    JSValue event = JS_NewObject(mContext);
    JS_SetPropertyStr(mContext, event, "itemComponentRegistry", itemRegistry);
    JS_SetPropertyStr(mContext, event, "blockComponentRegistry", blockRegistry);

    emitNamed("worldInitialize", event);
}

void ScriptApi::emitWorldLoad() {
    if (!hasNamedSubscribers("worldLoad"))
        return;

    emitNamed("worldLoad", JS_NewObject(mContext));
}

void ScriptApi::addNamedSubscriber(const std::string &name, JSValue callback) {
    mNamedSubscribers[name].push_back(callback);
}

void ScriptApi::removeNamedSubscriber(const std::string &name, JSValueConst callback) {
    auto it = mNamedSubscribers.find(name);
    if (it == mNamedSubscribers.end())
        return;

    std::vector<JSValue> &list = it->second;
    for (auto entry = list.begin(); entry != list.end(); ++entry) {
        if (JS_VALUE_GET_PTR(*entry) == JS_VALUE_GET_PTR(callback)) {
            JS_FreeValue(mContext, *entry);
            list.erase(entry);
            return;
        }
    }
}

bool ScriptApi::hasNamedSubscribers(const std::string &name) const {
    auto it = mNamedSubscribers.find(name);
    return it != mNamedSubscribers.end() && !it->second.empty();
}

bool ScriptApi::emitNamed(const std::string &name, JSValue eventObject) {
    auto it = mNamedSubscribers.find(name);
    if (it == mNamedSubscribers.end()) {
        JS_FreeValue(mContext, eventObject);
        return false;
    }

    const std::vector<JSValue> snapshot = it->second;
    for (JSValue callback: snapshot) {
        JSValue result = JS_Call(mContext, callback, JS_UNDEFINED, 1, &eventObject);
        if (JS_IsException(result)) {
            JSValue exception = JS_GetException(mContext);
            const char *message = JS_ToCString(mContext, exception);
            LOG_ERROR(LogAreaID::Server, "Script event handler error (%s): %s", name.c_str(),
                      message == nullptr ? "?" : message);
            if (message != nullptr)
                JS_FreeCString(mContext, message);
            JS_FreeValue(mContext, exception);
        }
        JS_FreeValue(mContext, result);
    }

    JSValue cancelValue = JS_GetPropertyStr(mContext, eventObject, "cancel");
    const bool cancelled = JS_ToBool(mContext, cancelValue) == 1;
    JS_FreeValue(mContext, cancelValue);

    JS_FreeValue(mContext, eventObject);
    return cancelled;
}

void ScriptApi::_dispatch(ScriptEvent event, JSValue eventObject) {
    const std::vector<JSValue> snapshot = mSubscribers[(int) event];

    for (JSValue callback: snapshot) {
        JSValue result = JS_Call(mContext, callback, JS_UNDEFINED, 1, &eventObject);
        if (JS_IsException(result)) {
            JSValue exception = JS_GetException(mContext);
            const char *message = JS_ToCString(mContext, exception);
            LOG_ERROR(LogAreaID::Server, "Script event handler error: %s", message == nullptr ? "?" : message);
            if (message != nullptr)
                JS_FreeCString(mContext, message);
            JS_FreeValue(mContext, exception);
        }
        JS_FreeValue(mContext, result);
    }

    JS_FreeValue(mContext, eventObject);
}

bool ScriptApi::_dispatchCancellable(ScriptEvent event, JSValue eventObject) {
    const std::vector<JSValue> snapshot = mSubscribers[(int) event];

    for (JSValue callback: snapshot) {
        JSValue result = JS_Call(mContext, callback, JS_UNDEFINED, 1, &eventObject);
        if (JS_IsException(result)) {
            JSValue exception = JS_GetException(mContext);
            const char *message = JS_ToCString(mContext, exception);
            LOG_ERROR(LogAreaID::Server, "Script event handler error: %s", message == nullptr ? "?" : message);
            if (message != nullptr)
                JS_FreeCString(mContext, message);
            JS_FreeValue(mContext, exception);
        }
        JS_FreeValue(mContext, result);
    }

    JSValue cancelValue = JS_GetPropertyStr(mContext, eventObject, "cancel");
    const bool cancelled = JS_ToBool(mContext, cancelValue) == 1;
    JS_FreeValue(mContext, cancelValue);

    JS_FreeValue(mContext, eventObject);
    return cancelled;
}

void ScriptApi::_subscribeGameEvents() {
    EventBus &bus = mHost.getEventBus();

    bus.after().mPlayerBreakBlock.subscribe([this](PlayerBreakBlockAfterEvent &event) {
        JSValue object = JS_NewObject(mContext);
        JS_SetPropertyStr(mContext, object, "player", makePlayer(event.mPlayer));
        JS_SetPropertyStr(mContext, object, "block",
                          makeVector3(mContext, (float) event.mBlockPosition.x, (float) event.mBlockPosition.y,
                                      (float) event.mBlockPosition.z));
        JS_SetPropertyStr(mContext, object, "brokenBlockPermutation",
                          makeBlockType(mContext, event.mBrokenBlockIdentifier));
        _dispatch(ScriptEvent::AfterPlayerBreakBlock, object);

        JSValue mineEvent = JS_NewObject(mContext);
        JS_SetPropertyStr(mContext, mineEvent, "source", makePlayer(event.mPlayer));
        JS_SetPropertyStr(mContext, mineEvent, "block",
                          makeVector3(mContext, (float) event.mBlockPosition.x, (float) event.mBlockPosition.y,
                                      (float) event.mBlockPosition.z));
        fireHeldItemComponents(event.mPlayer, "onMineBlock", mineEvent);
    });

    bus.after().mPlayerPlaceBlock.subscribe([this](PlayerPlaceBlockAfterEvent &event) {
        JSValue object = JS_NewObject(mContext);
        JS_SetPropertyStr(mContext, object, "player", makePlayer(event.mPlayer));
        JS_SetPropertyStr(mContext, object, "block",
                          makeVector3(mContext, (float) event.mBlockPosition.x, (float) event.mBlockPosition.y,
                                      (float) event.mBlockPosition.z));
        JS_SetPropertyStr(mContext, object, "permutation", makeBlockType(mContext, event.mBlockIdentifier));
        _dispatch(ScriptEvent::AfterPlayerPlaceBlock, object);
    });

    bus.after().mPlayerSpawn.subscribe([this](PlayerSpawnAfterEvent &event) {
        JSValue object = JS_NewObject(mContext);
        JS_SetPropertyStr(mContext, object, "player", makePlayer(event.mPlayer));
        JS_SetPropertyStr(mContext, object, "initialSpawn", JS_NewBool(mContext, event.mInitialSpawn));
        _dispatch(ScriptEvent::AfterPlayerSpawn, object);
    });

    bus.after().mPlayerJoin.subscribe([this](PlayerJoinAfterEvent &event) {
        JSValue object = JS_NewObject(mContext);
        JS_SetPropertyStr(mContext, object, "player", makePlayer(event.mPlayer));
        JS_SetPropertyStr(mContext, object, "playerName", jsString(mContext, event.mPlayer.getName()));
        _dispatch(ScriptEvent::AfterPlayerJoin, object);
    });

    bus.after().mPlayerLeave.subscribe([this](PlayerLeaveAfterEvent &event) {
        JSValue object = JS_NewObject(mContext);
        JS_SetPropertyStr(mContext, object, "playerName", jsString(mContext, event.mPlayerName));
        _dispatch(ScriptEvent::AfterPlayerLeave, object);
    });

    bus.after().mChatSend.subscribe([this](PlayerChatAfterEvent &event) {
        JSValue object = JS_NewObject(mContext);
        JS_SetPropertyStr(mContext, object, "sender", makePlayer(event.mSender));
        JS_SetPropertyStr(mContext, object, "message", jsString(mContext, event.mMessage));
        _dispatch(ScriptEvent::AfterChatSend, object);
    });

    bus.after().mEntityHurt.subscribe([this](EntityHurtAfterEvent &event) {
        JSValue object = JS_NewObject(mContext);
        JS_SetPropertyStr(mContext, object, "hurtEntity", makePlayer(event.mHurtEntity));
        JS_SetPropertyStr(mContext, object, "damage", JS_NewFloat64(mContext, event.mDamage));
        JS_SetPropertyStr(mContext, object, "damageSource", makeDamageSource(mContext, event.mCause));
        _dispatch(ScriptEvent::AfterEntityHurt, object);
    });

    bus.after().mEntityDie.subscribe([this](EntityDieAfterEvent &event) {
        JSValue object = JS_NewObject(mContext);
        JS_SetPropertyStr(mContext, object, "deadEntity", makePlayer(event.mDeadEntity));
        JS_SetPropertyStr(mContext, object, "damageSource", makeDamageSource(mContext, event.mCause));
        _dispatch(ScriptEvent::AfterEntityDie, object);
    });

    bus.after().mItemUse.subscribe([this](ItemUseAfterEvent &event) {
        JSValue object = JS_NewObject(mContext);
        JS_SetPropertyStr(mContext, object, "itemStack", makeHeldItemStack(event.mPlayer, event.mItemIdentifier));
        JS_SetPropertyStr(mContext, object, "source", makePlayer(event.mPlayer));
        _dispatch(ScriptEvent::AfterItemUse, object);
    });

    bus.before().mPlayerBreakBlock.subscribe([this](PlayerBreakBlockBeforeEvent &event) {
        JSValue object = JS_NewObject(mContext);
        JS_SetPropertyStr(mContext, object, "player", makePlayer(event.mPlayer));
        JS_SetPropertyStr(mContext, object, "block",
                          makeVector3(mContext, (float) event.mBlockPosition.x, (float) event.mBlockPosition.y,
                                      (float) event.mBlockPosition.z));
        JS_SetPropertyStr(mContext, object, "cancel", JS_NewBool(mContext, false));
        if (_dispatchCancellable(ScriptEvent::BeforePlayerBreakBlock, object))
            event.cancel();
    });

    bus.before().mChatSend.subscribe([this](PlayerChatBeforeEvent &event) {
        JSValue object = JS_NewObject(mContext);
        JS_SetPropertyStr(mContext, object, "sender", makePlayer(event.mSender));
        JS_SetPropertyStr(mContext, object, "message", jsString(mContext, event.mMessage));
        JS_SetPropertyStr(mContext, object, "cancel", JS_NewBool(mContext, false));
        if (_dispatchCancellable(ScriptEvent::BeforeChatSend, object))
            event.cancel();
    });
}

// ========================= Entity (Actor) =========================

ServerActor *ScriptApi::resolveActor(JSValueConst value) {
    void *opaque = JS_GetOpaque(value, mActorClassId);
    if (opaque == nullptr)
        return nullptr;

    const int64_t uniqueId = (int64_t) (intptr_t) opaque;
    return mHost.getActor(uniqueId);
}

JSValue ScriptApi::makeActor(ServerActor &actor) {
    JSValue object = JS_NewObjectProtoClass(mContext, mActorPrototype, mActorClassId);
    JS_SetOpaque(object, (void *) (intptr_t) actor.getUniqueId());
    return object;
}

namespace {
    bool arrayContainsString(JSContext *ctx, JSValueConst array, const std::string &needle) {
        if (!JS_IsArray(array))
            return false;
        uint32_t length = 0;
        JSValue lengthValue = JS_GetPropertyStr(ctx, array, "length");
        JS_ToUint32(ctx, &length, lengthValue);
        JS_FreeValue(ctx, lengthValue);

        for (uint32_t i = 0; i < length; ++i) {
            JSValue element = JS_GetPropertyUint32(ctx, array, i);
            const std::string value = toStdString(ctx, element);
            JS_FreeValue(ctx, element);
            if (value == needle)
                return true;
        }
        return false;
    }

    bool actorMatchesQuery(JSContext *ctx, ServerActor &actor, JSValueConst options) {
        if (!JS_IsObject(options))
            return true;

        JSValue typeValue = JS_GetPropertyStr(ctx, options, "type");
        if (JS_IsString(typeValue)) {
            const std::string type = toStdString(ctx, typeValue);
            if (!type.empty() && type != actor.getTypeId()) {
                JS_FreeValue(ctx, typeValue);
                return false;
            }
        }
        JS_FreeValue(ctx, typeValue);

        const CustomActorDefinition *definition = actor.getDefinition();

        JSValue excludeTypes = JS_GetPropertyStr(ctx, options, "excludeTypes");
        const bool typeExcluded = arrayContainsString(ctx, excludeTypes, actor.getTypeId());
        JS_FreeValue(ctx, excludeTypes);
        if (typeExcluded)
            return false;

        JSValue excludeFamilies = JS_GetPropertyStr(ctx, options, "excludeFamilies");
        if (JS_IsArray(excludeFamilies) && definition != nullptr) {
            for (const std::string &family: definition->mTypeFamilies) {
                if (arrayContainsString(ctx, excludeFamilies, family)) {
                    JS_FreeValue(ctx, excludeFamilies);
                    return false;
                }
            }
        }
        JS_FreeValue(ctx, excludeFamilies);

        JSValue excludeTags = JS_GetPropertyStr(ctx, options, "excludeTags");
        if (JS_IsArray(excludeTags)) {
            for (const std::string &tag: actor.getTags()) {
                if (arrayContainsString(ctx, excludeTags, tag)) {
                    JS_FreeValue(ctx, excludeTags);
                    return false;
                }
            }
        }
        JS_FreeValue(ctx, excludeTags);

        JSValue families = JS_GetPropertyStr(ctx, options, "families");
        if (JS_IsArray(families)) {
            uint32_t length = 0;
            JSValue lengthValue = JS_GetPropertyStr(ctx, families, "length");
            JS_ToUint32(ctx, &length, lengthValue);
            JS_FreeValue(ctx, lengthValue);

            for (uint32_t i = 0; i < length; ++i) {
                JSValue element = JS_GetPropertyUint32(ctx, families, i);
                const std::string family = toStdString(ctx, element);
                JS_FreeValue(ctx, element);

                bool found = false;
                if (definition != nullptr) {
                    for (const std::string &candidate: definition->mTypeFamilies) {
                        if (candidate == family) {
                            found = true;
                            break;
                        }
                    }
                }
                if (!found) {
                    JS_FreeValue(ctx, families);
                    return false;
                }
            }
        }
        JS_FreeValue(ctx, families);

        JSValue tags = JS_GetPropertyStr(ctx, options, "tags");
        if (JS_IsArray(tags)) {
            uint32_t length = 0;
            JSValue lengthValue = JS_GetPropertyStr(ctx, tags, "length");
            JS_ToUint32(ctx, &length, lengthValue);
            JS_FreeValue(ctx, lengthValue);

            for (uint32_t i = 0; i < length; ++i) {
                JSValue element = JS_GetPropertyUint32(ctx, tags, i);
                const std::string tag = toStdString(ctx, element);
                JS_FreeValue(ctx, element);

                if (!actor.hasTag(tag)) {
                    JS_FreeValue(ctx, tags);
                    return false;
                }
            }
        }
        JS_FreeValue(ctx, tags);

        return true;
    }

    ServerActor *resolveActorThis(JSContext *ctx, JSValueConst thisVal) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        return api->resolveActor(thisVal);
    }

    JSValue actorGetTypeId(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerActor *actor = resolveActorThis(ctx, thisVal);
        if (actor == nullptr)
            return JS_ThrowTypeError(ctx, "Entity is not valid");
        return jsString(ctx, actor->getTypeId());
    }

    JSValue actorGetId(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerActor *actor = resolveActorThis(ctx, thisVal);
        if (actor == nullptr)
            return JS_ThrowTypeError(ctx, "Entity is not valid");
        return jsString(ctx, std::to_string(actor->getUniqueId()));
    }

    JSValue actorGetIsValid(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewBool(ctx, resolveActorThis(ctx, thisVal) != nullptr);
    }

    JSValue actorGetLocation(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerActor *actor = resolveActorThis(ctx, thisVal);
        if (actor == nullptr)
            return JS_ThrowTypeError(ctx, "Entity is not valid");
        const Vector3f &position = actor->getPosition();
        return makeVector3(ctx, position.x, position.y, position.z);
    }

    JSValue actorGetDimension(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerActor *actor = api->resolveActor(thisVal);
        if (actor == nullptr)
            return JS_ThrowTypeError(ctx, "Entity is not valid");
        return api->makeDimension(actor->getDimension());
    }

    JSValue actorRemove(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerActor *actor = api->resolveActor(thisVal);
        if (actor == nullptr)
            return JS_UNDEFINED;
        api->host().removeActor(actor->getUniqueId());
        return JS_UNDEFINED;
    }

    JSValue actorTeleport(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerActor *actor = api->resolveActor(thisVal);
        if (actor == nullptr)
            return JS_ThrowTypeError(ctx, "Entity is not valid");

        double x = 0.0, y = 0.0, z = 0.0;
        if (argc < 1 || !readVector3(ctx, argv[0], x, y, z))
            return JS_ThrowTypeError(ctx, "teleport requires a location {x,y,z}");

        actor->setPosition(Vector3f((float) x, (float) y, (float) z));

        if (argc >= 2 && JS_IsObject(argv[1])) {
            JSValue rotationValue = JS_GetPropertyStr(ctx, argv[1], "rotation");
            if (JS_IsObject(rotationValue)) {
                JSValue rx = JS_GetPropertyStr(ctx, rotationValue, "x");
                JSValue ry = JS_GetPropertyStr(ctx, rotationValue, "y");
                double pitch = 0.0, yaw = 0.0;
                JS_ToFloat64(ctx, &pitch, rx);
                JS_ToFloat64(ctx, &yaw, ry);
                actor->setRotation(Vector3f((float) pitch, (float) yaw, 0.0f));
                JS_FreeValue(ctx, rx);
                JS_FreeValue(ctx, ry);
            }
            JS_FreeValue(ctx, rotationValue);
        }

        api->host().broadcastActorMove(*actor);
        return JS_UNDEFINED;
    }

    JSValue actorGetRotation(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerActor *actor = resolveActorThis(ctx, thisVal);
        if (actor == nullptr)
            return JS_ThrowTypeError(ctx, "Entity is not valid");
        JSValue object = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, object, "x", JS_NewFloat64(ctx, actor->getRotation().x));
        JS_SetPropertyStr(ctx, object, "y", JS_NewFloat64(ctx, actor->getRotation().y));
        return object;
    }

    JSValue actorGetVelocity(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerActor *actor = resolveActorThis(ctx, thisVal);
        if (actor == nullptr)
            return JS_ThrowTypeError(ctx, "Entity is not valid");
        const Vector3f &motion = actor->getMotion();
        return makeVector3(ctx, motion.x, motion.y, motion.z);
    }

    JSValue actorClearVelocity(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerActor *actor = resolveActorThis(ctx, thisVal);
        if (actor == nullptr)
            return JS_UNDEFINED;
        actor->setMotion(Vector3f(0.0f, 0.0f, 0.0f));
        return JS_UNDEFINED;
    }

    JSValue actorApplyImpulse(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerActor *actor = resolveActorThis(ctx, thisVal);
        if (actor == nullptr)
            return JS_ThrowTypeError(ctx, "Entity is not valid");

        double x = 0.0, y = 0.0, z = 0.0;
        if (argc >= 1 && readVector3(ctx, argv[0], x, y, z)) {
            const Vector3f &motion = actor->getMotion();
            actor->setMotion(Vector3f(motion.x + (float) x, motion.y + (float) y, motion.z + (float) z));
        }
        return JS_UNDEFINED;
    }

    JSValue actorApplyDamage(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerActor *actor = api->resolveActor(thisVal);
        if (actor == nullptr)
            return JS_NewBool(ctx, false);

        double amount = 0.0;
        if (argc >= 1)
            JS_ToFloat64(ctx, &amount, argv[0]);

        actor->setHealth(actor->getHealth() - (float) amount);
        if (actor->getHealth() <= 0.0f)
            api->host().removeActor(actor->getUniqueId());
        return JS_NewBool(ctx, true);
    }

    JSValue actorKill(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerActor *actor = api->resolveActor(thisVal);
        if (actor == nullptr)
            return JS_NewBool(ctx, false);
        api->host().removeActor(actor->getUniqueId());
        return JS_NewBool(ctx, true);
    }

    JSValue actorAddEffect(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerActor *actor = api->resolveActor(thisVal);
        if (actor == nullptr || argc < 1)
            return JS_UNDEFINED;

        MobEffectId id;
        if (!parseMobEffect(toStdString(ctx, argv[0]), id))
            return JS_UNDEFINED;

        int32_t duration = 400;
        if (argc >= 2)
            JS_ToInt32(ctx, &duration, argv[1]);

        int32_t amplifier = 0;
        bool showParticles = true;
        if (argc >= 3 && JS_IsObject(argv[2])) {
            JSValue amplifierValue = JS_GetPropertyStr(ctx, argv[2], "amplifier");
            if (!JS_IsUndefined(amplifierValue))
                JS_ToInt32(ctx, &amplifier, amplifierValue);
            JS_FreeValue(ctx, amplifierValue);

            JSValue particlesValue = JS_GetPropertyStr(ctx, argv[2], "showParticles");
            if (!JS_IsUndefined(particlesValue))
                showParticles = JS_ToBool(ctx, particlesValue) == 1;
            JS_FreeValue(ctx, particlesValue);
        }

        MobEffectInstance instance;
        instance.mId = id;
        instance.mDuration = duration;
        instance.mAmplifier = amplifier;
        actor->addEffect(instance);
        api->host().applyActorEffect(*actor, (int32_t) id, amplifier, duration, showParticles);
        return JS_UNDEFINED;
    }

    JSValue actorSetRotation(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerActor *actor = api->resolveActor(thisVal);
        if (actor == nullptr || argc < 1 || !JS_IsObject(argv[0]))
            return JS_UNDEFINED;

        JSValue xValue = JS_GetPropertyStr(ctx, argv[0], "x");
        JSValue yValue = JS_GetPropertyStr(ctx, argv[0], "y");
        double pitch = 0.0, yaw = 0.0;
        JS_ToFloat64(ctx, &pitch, xValue);
        JS_ToFloat64(ctx, &yaw, yValue);
        JS_FreeValue(ctx, xValue);
        JS_FreeValue(ctx, yValue);

        actor->setRotation(Vector3f((float) pitch, (float) yaw, 0.0f));
        api->host().broadcastActorMove(*actor);
        return JS_UNDEFINED;
    }

    JSValue actorSetOnFire(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerActor *actor = api->resolveActor(thisVal);
        if (actor == nullptr)
            return JS_NewBool(ctx, false);

        int32_t seconds = 1;
        if (argc >= 1)
            JS_ToInt32(ctx, &seconds, argv[0]);
        actor->setFireTicks(seconds * 20);
        return JS_NewBool(ctx, true);
    }

    JSValue actorExtinguishFire(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerActor *actor = api->resolveActor(thisVal);
        if (actor != nullptr)
            actor->extinguish();
        return JS_NewBool(ctx, true);
    }

    JSValue actorApplyKnockback(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerActor *actor = resolveActorThis(ctx, thisVal);
        if (actor == nullptr)
            return JS_UNDEFINED;

        double dx = 0.0, dz = 0.0, vertical = 0.0;
        if (argc >= 1 && JS_IsObject(argv[0])) {
            JSValue xValue = JS_GetPropertyStr(ctx, argv[0], "x");
            JSValue zValue = JS_GetPropertyStr(ctx, argv[0], "z");
            JS_ToFloat64(ctx, &dx, xValue);
            JS_ToFloat64(ctx, &dz, zValue);
            JS_FreeValue(ctx, xValue);
            JS_FreeValue(ctx, zValue);
            if (argc >= 2)
                JS_ToFloat64(ctx, &vertical, argv[1]);
        } else if (argc >= 4) {
            double strength = 0.0;
            JS_ToFloat64(ctx, &dx, argv[0]);
            JS_ToFloat64(ctx, &dz, argv[1]);
            JS_ToFloat64(ctx, &strength, argv[2]);
            JS_ToFloat64(ctx, &vertical, argv[3]);
            dx *= strength;
            dz *= strength;
        }

        const Vector3f &motion = actor->getMotion();
        actor->setMotion(Vector3f(motion.x + (float) dx, (float) vertical, motion.z + (float) dz));
        return JS_UNDEFINED;
    }

    JSValue effectToJs(JSContext *ctx, const MobEffectInstance &effect) {
        const std::string identifier = "minecraft:" + std::string(getMobEffectName(effect.mId));
        JSValue object = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, object, "typeId", jsString(ctx, identifier));
        JS_SetPropertyStr(ctx, object, "amplifier", JS_NewInt32(ctx, effect.mAmplifier));
        JS_SetPropertyStr(ctx, object, "duration", JS_NewInt32(ctx, effect.mDuration));
        return object;
    }

    JSValue actorGetEffect(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerActor *actor = resolveActorThis(ctx, thisVal);
        if (actor == nullptr || argc < 1)
            return JS_UNDEFINED;

        MobEffectId id;
        if (!parseMobEffect(toStdString(ctx, argv[0]), id))
            return JS_UNDEFINED;

        const MobEffectInstance *effect = actor->getEffect(id);
        if (effect == nullptr)
            return JS_UNDEFINED;
        return effectToJs(ctx, *effect);
    }

    JSValue actorRemoveEffect(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerActor *actor = api->resolveActor(thisVal);
        if (actor == nullptr || argc < 1)
            return JS_NewBool(ctx, false);

        MobEffectId id;
        if (!parseMobEffect(toStdString(ctx, argv[0]), id))
            return JS_NewBool(ctx, false);

        const bool removed = actor->removeEffect(id);
        if (removed)
            api->host().applyActorEffect(*actor, (int32_t) id, 0, 0, false);
        return JS_NewBool(ctx, removed);
    }

    JSValue actorGetNameTag(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerActor *actor = resolveActorThis(ctx, thisVal);
        if (actor == nullptr)
            return JS_NewString(ctx, "");
        return jsString(ctx, actor->getNameTag());
    }

    JSValue actorSetNameTag(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerActor *actor = resolveActorThis(ctx, thisVal);
        if (actor != nullptr && argc >= 1) {
            actor->setNameTag(toStdString(ctx, argv[0]));
            ScriptApi::fromRuntime(JS_GetRuntime(ctx))->host().sendActorNameTag(*actor);
        }
        return JS_UNDEFINED;
    }

    JSValue actorPlayAnimation(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerActor *actor = api->resolveActor(thisVal);
        if (actor == nullptr)
            return JS_UNDEFINED;
        if (argc >= 1)
            api->host().playActorAnimation(*actor, toStdString(ctx, argv[0]));
        return JS_UNDEFINED;
    }

    JSValue actorRunCommand(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (api->resolveActor(thisVal) == nullptr)
            return JS_ThrowTypeError(ctx, "Entity is not valid");

        const std::string line = argc >= 1 ? toStdString(ctx, argv[0]) : std::string();
        const int32_t success = api->runCommandAsConsole(line);

        JSValue result = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, result, "successCount", JS_NewInt32(ctx, success));
        return result;
    }

    JSValue actorAddTag(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerActor *actor = resolveActorThis(ctx, thisVal);
        if (actor == nullptr || argc < 1)
            return JS_NewBool(ctx, false);
        actor->addTag(toStdString(ctx, argv[0]));
        return JS_NewBool(ctx, true);
    }

    JSValue actorRemoveTag(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerActor *actor = resolveActorThis(ctx, thisVal);
        if (actor == nullptr || argc < 1)
            return JS_NewBool(ctx, false);
        return JS_NewBool(ctx, actor->removeTag(toStdString(ctx, argv[0])));
    }

    JSValue actorHasTag(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerActor *actor = resolveActorThis(ctx, thisVal);
        if (actor == nullptr || argc < 1)
            return JS_NewBool(ctx, false);
        return JS_NewBool(ctx, actor->hasTag(toStdString(ctx, argv[0])));
    }

    JSValue actorGetTags(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerActor *actor = resolveActorThis(ctx, thisVal);
        JSValue array = JS_NewArray(ctx);
        if (actor == nullptr)
            return array;
        uint32_t index = 0;
        for (const std::string &tag: actor->getTags())
            JS_SetPropertyUint32(ctx, array, index++, jsString(ctx, tag));
        return array;
    }

    JSValue actorMatches(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerActor *actor = resolveActorThis(ctx, thisVal);
        if (actor == nullptr)
            return JS_NewBool(ctx, false);
        const bool matches = argc >= 1 && actorMatchesQuery(ctx, *actor, argv[0]);
        return JS_NewBool(ctx, matches);
    }

    JSValue actorGetProperty(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerActor *actor = resolveActorThis(ctx, thisVal);
        if (actor == nullptr || argc < 1)
            return JS_UNDEFINED;

        const std::string name = toStdString(ctx, argv[0]);
        const CustomActorDefinition *definition = actor->getDefinition();
        const ActorPropertyDescription *descriptor = definition == nullptr ? nullptr
                                                                           : definition->findProperty(name);
        if (descriptor == nullptr)
            return JS_UNDEFINED;

        if (descriptor->mType == ActorPropertyDescription::Type::Float)
            return JS_NewFloat64(ctx, actor->getFloatProperty(name, descriptor->mDefaultFloat));
        if (descriptor->mType == ActorPropertyDescription::Type::Bool)
            return JS_NewBool(ctx, actor->getIntProperty(name, descriptor->mDefaultInt) != 0);
        if (descriptor->mType == ActorPropertyDescription::Type::Enum) {
            const int32_t index = actor->getIntProperty(name, descriptor->mDefaultInt);
            if (index >= 0 && index < (int32_t) descriptor->mEnumValues.size())
                return jsString(ctx, descriptor->mEnumValues[index]);
            return JS_UNDEFINED;
        }
        return JS_NewInt32(ctx, actor->getIntProperty(name, descriptor->mDefaultInt));
    }

    JSValue actorSetProperty(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerActor *actor = api->resolveActor(thisVal);
        if (actor == nullptr || argc < 2)
            return JS_UNDEFINED;

        const std::string name = toStdString(ctx, argv[0]);
        const CustomActorDefinition *definition = actor->getDefinition();
        const ActorPropertyDescription *descriptor = definition == nullptr ? nullptr
                                                                           : definition->findProperty(name);
        if (descriptor == nullptr)
            return JS_UNDEFINED;

        if (descriptor->mType == ActorPropertyDescription::Type::Float) {
            double value = 0.0;
            JS_ToFloat64(ctx, &value, argv[1]);
            actor->setFloatProperty(name, (float) value);
        } else if (descriptor->mType == ActorPropertyDescription::Type::Bool) {
            actor->setIntProperty(name, JS_ToBool(ctx, argv[1]) == 1 ? 1 : 0);
        } else if (descriptor->mType == ActorPropertyDescription::Type::Enum) {
            const std::string value = toStdString(ctx, argv[1]);
            for (size_t i = 0; i < descriptor->mEnumValues.size(); ++i) {
                if (descriptor->mEnumValues[i] == value)
                    actor->setIntProperty(name, (int32_t) i);
            }
        } else {
            int32_t value = 0;
            JS_ToInt32(ctx, &value, argv[1]);
            actor->setIntProperty(name, value);
        }

        api->host().syncActorProperties(*actor);
        return JS_UNDEFINED;
    }

    JSValue actorProjectileShoot(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        JSValue actorValue = JS_GetPropertyStr(ctx, thisVal, "_entity");
        ServerActor *actor = api->resolveActor(actorValue);
        JS_FreeValue(ctx, actorValue);
        if (actor == nullptr)
            return JS_UNDEFINED;

        double x = 0.0, y = 0.0, z = 0.0;
        if (argc >= 1 && readVector3(ctx, argv[0], x, y, z)) {
            actor->setMotion(Vector3f((float) x, (float) y, (float) z));
            actor->setProjectile(true);
        }

        JSValue ownerValue = JS_GetPropertyStr(ctx, thisVal, "owner");
        if (JS_IsObject(ownerValue)) {
            void *ownerOpaque = JS_GetOpaque(ownerValue, api->playerClassId());
            if (ownerOpaque != nullptr)
                actor->setOwnerPlayerHandle((uint32_t) (uintptr_t) ownerOpaque);
            else {
                ServerActor *ownerActor = api->resolveActor(ownerValue);
                if (ownerActor != nullptr)
                    actor->setOwnerUniqueId(ownerActor->getUniqueId());
            }
        }
        JS_FreeValue(ctx, ownerValue);
        return JS_UNDEFINED;
    }

    JSValue actorTypeFamilyHas(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_NewBool(ctx, false);

        const std::string wanted = toStdString(ctx, argv[0]);
        JSValue families = JS_GetPropertyStr(ctx, thisVal, "_families");
        uint32_t length = 0;
        JSValue lengthValue = JS_GetPropertyStr(ctx, families, "length");
        JS_ToUint32(ctx, &length, lengthValue);
        JS_FreeValue(ctx, lengthValue);

        bool found = false;
        for (uint32_t i = 0; i < length; ++i) {
            JSValue element = JS_GetPropertyUint32(ctx, families, i);
            if (toStdString(ctx, element) == wanted)
                found = true;
            JS_FreeValue(ctx, element);
            if (found)
                break;
        }
        JS_FreeValue(ctx, families);
        return JS_NewBool(ctx, found);
    }

    JSValue makeHealthComponent(JSContext *ctx, ScriptApi *api, ServerActor &actor);

    JSValue actorHasComponent(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerActor *actor = api->resolveActor(thisVal);
        if (actor == nullptr || argc < 1)
            return JS_NewBool(ctx, false);

        const std::string id = toStdString(ctx, argv[0]);
        const CustomActorDefinition *definition = actor->getDefinition();

        if (id == "minecraft:health" || id == "health")
            return JS_NewBool(ctx, true);
        if (id == "minecraft:type_family" || id == "type_family")
            return JS_NewBool(ctx, definition != nullptr && !definition->mTypeFamilies.empty());
        if (id == "minecraft:projectile" || id == "projectile")
            return JS_NewBool(ctx, definition != nullptr && definition->mIsProjectile);

        return JS_NewBool(ctx, false);
    }

    JSValue actorGetComponents(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerActor *actor = api->resolveActor(thisVal);
        JSValue array = JS_NewArray(ctx);
        if (actor == nullptr)
            return array;

        uint32_t index = 0;
        JS_SetPropertyUint32(ctx, array, index++, makeHealthComponent(ctx, api, *actor));
        return array;
    }

    ServerActor *healthComponentActor(JSContext *ctx, JSValueConst thisVal) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        JSValue entityValue = JS_GetPropertyStr(ctx, thisVal, "_entity");
        int64_t uniqueId = 0;
        JS_ToInt64(ctx, &uniqueId, entityValue);
        JS_FreeValue(ctx, entityValue);
        return api->host().getActor(uniqueId);
    }

    JSValue healthGetCurrent(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerActor *actor = healthComponentActor(ctx, thisVal);
        return JS_NewFloat64(ctx, actor == nullptr ? 0.0 : actor->getHealth());
    }

    JSValue healthSetCurrent(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerActor *actor = healthComponentActor(ctx, thisVal);
        if (actor != nullptr && argc >= 1) {
            double value = 0.0;
            JS_ToFloat64(ctx, &value, argv[0]);
            actor->setHealth((float) value);
        }
        return JS_UNDEFINED;
    }

    JSValue healthResetToMax(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerActor *actor = healthComponentActor(ctx, thisVal);
        if (actor != nullptr)
            actor->setHealth(actor->getMaxHealth());
        return JS_UNDEFINED;
    }

    JSValue makeHealthComponent(JSContext *ctx, ScriptApi *api, ServerActor &actor) {
        JSValue component = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, component, "typeId", JS_NewString(ctx, "minecraft:health"));
        JS_SetPropertyStr(ctx, component, "_entity", JS_NewInt64(ctx, actor.getUniqueId()));
        JS_SetPropertyStr(ctx, component, "effectiveMax", JS_NewFloat64(ctx, actor.getMaxHealth()));
        JS_SetPropertyStr(ctx, component, "defaultValue", JS_NewFloat64(ctx, actor.getMaxHealth()));
        JS_SetPropertyStr(ctx, component, "setCurrentValue",
                          JS_NewCFunction(ctx, healthSetCurrent, "setCurrentValue", 1));
        JS_SetPropertyStr(ctx, component, "resetToMaxValue",
                          JS_NewCFunction(ctx, healthResetToMax, "resetToMaxValue", 0));

        JSAtom currentAtom = JS_NewAtom(ctx, "currentValue");
        JS_DefinePropertyGetSet(ctx, component, currentAtom,
                                JS_NewCFunction(ctx, healthGetCurrent, "get currentValue", 0), JS_UNDEFINED,
                                JS_PROP_CONFIGURABLE);
        JS_FreeAtom(ctx, currentAtom);
        return component;
    }

    JSValue actorGetComponent(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ServerActor *actor = api->resolveActor(thisVal);
        if (actor == nullptr || argc < 1)
            return JS_UNDEFINED;

        const std::string id = toStdString(ctx, argv[0]);
        const CustomActorDefinition *definition = actor->getDefinition();

        if (id == "minecraft:health" || id == "health")
            return makeHealthComponent(ctx, api, *actor);

        if (id == "minecraft:type_family" || id == "type_family") {
            JSValue component = JS_NewObject(ctx);
            JSValue families = JS_NewArray(ctx);
            uint32_t index = 0;
            if (definition != nullptr) {
                for (const std::string &family: definition->mTypeFamilies)
                    JS_SetPropertyUint32(ctx, families, index++, jsString(ctx, family));
            }
            JS_SetPropertyStr(ctx, component, "_families", families);
            JS_SetPropertyStr(ctx, component, "hasTypeFamily",
                              JS_NewCFunction(ctx, actorTypeFamilyHas, "hasTypeFamily", 1));
            return component;
        }

        if (id == "minecraft:projectile" || id == "projectile") {
            if (definition == nullptr || !definition->mIsProjectile)
                return JS_UNDEFINED;
            JSValue component = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, component, "_entity", api->makeActor(*actor));
            if (actor->getOwnerUniqueId() >= 0) {
                ServerActor *owner = api->host().getActor(actor->getOwnerUniqueId());
                if (owner != nullptr)
                    JS_SetPropertyStr(ctx, component, "owner", api->makeActor(*owner));
            }
            JS_SetPropertyStr(ctx, component, "shoot",
                              JS_NewCFunction(ctx, actorProjectileShoot, "shoot", 1));
            return component;
        }

        return JS_UNDEFINED;
    }

    JSValue actorGetDynamicProperty(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerActor *actor = resolveActorThis(ctx, thisVal);
        if (actor == nullptr || argc < 1)
            return JS_UNDEFINED;
        return dynamicPropertyGet(ctx, actor->getDynamicProperties(), toStdString(ctx, argv[0]));
    }

    JSValue actorSetDynamicProperty(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ServerActor *actor = resolveActorThis(ctx, thisVal);
        if (actor == nullptr || argc < 1)
            return JS_UNDEFINED;
        dynamicPropertySet(ctx, actor->getDynamicProperties(), toStdString(ctx, argv[0]),
                           argc >= 2 ? argv[1] : JS_UNDEFINED);
        return JS_UNDEFINED;
    }

    JSValue actorGetDynamicPropertyIds(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerActor *actor = resolveActorThis(ctx, thisVal);
        if (actor == nullptr)
            return JS_NewArray(ctx);
        return dynamicPropertyIds(ctx, actor->getDynamicProperties());
    }

    JSValue actorClearDynamicProperties(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ServerActor *actor = resolveActorThis(ctx, thisVal);
        if (actor != nullptr)
            actor->getDynamicProperties().clear();
        return JS_UNDEFINED;
    }
}

void ScriptApi::_buildActorClass() {
    JS_NewClassID(mRuntime, &mActorClassId);

    JSClassDef actorClass{};
    actorClass.class_name = "Entity";
    JS_NewClass(mRuntime, mActorClassId, &actorClass);

    mActorPrototype = JS_NewObject(mContext);

    JS_SetPropertyStr(mContext, mActorPrototype, "remove",
                      JS_NewCFunction(mContext, actorRemove, "remove", 0));
    JS_SetPropertyStr(mContext, mActorPrototype, "teleport",
                      JS_NewCFunction(mContext, actorTeleport, "teleport", 1));
    JS_SetPropertyStr(mContext, mActorPrototype, "getRotation",
                      JS_NewCFunction(mContext, actorGetRotation, "getRotation", 0));
    JS_SetPropertyStr(mContext, mActorPrototype, "getVelocity",
                      JS_NewCFunction(mContext, actorGetVelocity, "getVelocity", 0));
    JS_SetPropertyStr(mContext, mActorPrototype, "clearVelocity",
                      JS_NewCFunction(mContext, actorClearVelocity, "clearVelocity", 0));
    JS_SetPropertyStr(mContext, mActorPrototype, "applyImpulse",
                      JS_NewCFunction(mContext, actorApplyImpulse, "applyImpulse", 1));
    JS_SetPropertyStr(mContext, mActorPrototype, "applyDamage",
                      JS_NewCFunction(mContext, actorApplyDamage, "applyDamage", 1));
    JS_SetPropertyStr(mContext, mActorPrototype, "kill",
                      JS_NewCFunction(mContext, actorKill, "kill", 0));
    JS_SetPropertyStr(mContext, mActorPrototype, "addEffect",
                      JS_NewCFunction(mContext, actorAddEffect, "addEffect", 3));
    JS_SetPropertyStr(mContext, mActorPrototype, "removeEffect",
                      JS_NewCFunction(mContext, actorRemoveEffect, "removeEffect", 1));
    JS_SetPropertyStr(mContext, mActorPrototype, "getEffect",
                      JS_NewCFunction(mContext, actorGetEffect, "getEffect", 1));
    JS_SetPropertyStr(mContext, mActorPrototype, "setRotation",
                      JS_NewCFunction(mContext, actorSetRotation, "setRotation", 1));
    JS_SetPropertyStr(mContext, mActorPrototype, "setOnFire",
                      JS_NewCFunction(mContext, actorSetOnFire, "setOnFire", 1));
    JS_SetPropertyStr(mContext, mActorPrototype, "extinguishFire",
                      JS_NewCFunction(mContext, actorExtinguishFire, "extinguishFire", 0));
    JS_SetPropertyStr(mContext, mActorPrototype, "applyKnockback",
                      JS_NewCFunction(mContext, actorApplyKnockback, "applyKnockback", 2));

    JSAtom nameTagAtom = JS_NewAtom(mContext, "nameTag");
    JS_DefinePropertyGetSet(mContext, mActorPrototype, nameTagAtom,
                            JS_NewCFunction(mContext, actorGetNameTag, "get nameTag", 0),
                            JS_NewCFunction(mContext, actorSetNameTag, "set nameTag", 1), JS_PROP_CONFIGURABLE);
    JS_FreeAtom(mContext, nameTagAtom);
    JS_SetPropertyStr(mContext, mActorPrototype, "playAnimation",
                      JS_NewCFunction(mContext, actorPlayAnimation, "playAnimation", 1));
    JS_SetPropertyStr(mContext, mActorPrototype, "runCommand",
                      JS_NewCFunction(mContext, actorRunCommand, "runCommand", 1));
    JS_SetPropertyStr(mContext, mActorPrototype, "addTag",
                      JS_NewCFunction(mContext, actorAddTag, "addTag", 1));
    JS_SetPropertyStr(mContext, mActorPrototype, "removeTag",
                      JS_NewCFunction(mContext, actorRemoveTag, "removeTag", 1));
    JS_SetPropertyStr(mContext, mActorPrototype, "hasTag",
                      JS_NewCFunction(mContext, actorHasTag, "hasTag", 1));
    JS_SetPropertyStr(mContext, mActorPrototype, "getTags",
                      JS_NewCFunction(mContext, actorGetTags, "getTags", 0));
    JS_SetPropertyStr(mContext, mActorPrototype, "matches",
                      JS_NewCFunction(mContext, actorMatches, "matches", 1));
    JS_SetPropertyStr(mContext, mActorPrototype, "getProperty",
                      JS_NewCFunction(mContext, actorGetProperty, "getProperty", 1));
    JS_SetPropertyStr(mContext, mActorPrototype, "setProperty",
                      JS_NewCFunction(mContext, actorSetProperty, "setProperty", 2));
    JS_SetPropertyStr(mContext, mActorPrototype, "getComponent",
                      JS_NewCFunction(mContext, actorGetComponent, "getComponent", 1));
    JS_SetPropertyStr(mContext, mActorPrototype, "hasComponent",
                      JS_NewCFunction(mContext, actorHasComponent, "hasComponent", 1));
    JS_SetPropertyStr(mContext, mActorPrototype, "getComponents",
                      JS_NewCFunction(mContext, actorGetComponents, "getComponents", 0));
    JS_SetPropertyStr(mContext, mActorPrototype, "getDynamicProperty",
                      JS_NewCFunction(mContext, actorGetDynamicProperty, "getDynamicProperty", 1));
    JS_SetPropertyStr(mContext, mActorPrototype, "setDynamicProperty",
                      JS_NewCFunction(mContext, actorSetDynamicProperty, "setDynamicProperty", 2));
    JS_SetPropertyStr(mContext, mActorPrototype, "getDynamicPropertyIds",
                      JS_NewCFunction(mContext, actorGetDynamicPropertyIds, "getDynamicPropertyIds", 0));
    JS_SetPropertyStr(mContext, mActorPrototype, "clearDynamicProperties",
                      JS_NewCFunction(mContext, actorClearDynamicProperties, "clearDynamicProperties", 0));

    static const char *coordAtoms[] = {"typeId", "id", "isValid", "location", "dimension"};
    JSCFunction *getters[] = {actorGetTypeId, actorGetId, actorGetIsValid, actorGetLocation, actorGetDimension};
    for (int i = 0; i < 5; ++i) {
        JSAtom atom = JS_NewAtom(mContext, coordAtoms[i]);
        JS_DefinePropertyGetSet(mContext, mActorPrototype, atom,
                                JS_NewCFunction(mContext, getters[i], "get", 0), JS_UNDEFINED,
                                JS_PROP_CONFIGURABLE);
        JS_FreeAtom(mContext, atom);
    }

    JS_SetClassProto(mContext, mActorClassId, JS_DupValue(mContext, mActorPrototype));
}

// ========================= Block =========================

namespace {
    struct ScriptBlockData {
        DimensionType mDimension = DimensionType::Overworld;
        int32_t mX = 0;
        int32_t mY = 0;
        int32_t mZ = 0;
    };

    ScriptBlockData *blockData(JSValueConst value, JSClassID classId) {
        return (ScriptBlockData *) JS_GetOpaque(value, classId);
    }

    JSValue blockGetCoord(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ScriptBlockData *data = blockData(thisVal, api->blockClassId());
        if (data == nullptr)
            return JS_ThrowTypeError(ctx, "Block is not valid");

        const int32_t value = magic == 0 ? data->mX : (magic == 1 ? data->mY : data->mZ);
        return JS_NewInt32(ctx, value);
    }

    std::string blockIdentifierAt(ScriptApi *api, ScriptBlockData *data) {
        return api->host().getDimension(data->mDimension).getBlockState(data->mX, data->mY, data->mZ).mName;
    }

    JSValue blockGetTypeId(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ScriptBlockData *data = blockData(thisVal, api->blockClassId());
        if (data == nullptr)
            return JS_ThrowTypeError(ctx, "Block is not valid");

        return jsString(ctx, blockIdentifierAt(api, data));
    }

    JSValue blockGetIsAir(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ScriptBlockData *data = blockData(thisVal, api->blockClassId());
        if (data == nullptr)
            return JS_ThrowTypeError(ctx, "Block is not valid");

        return JS_NewBool(ctx, blockIdentifierAt(api, data) == "minecraft:air");
    }

    JSValue blockGetPermutation(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ScriptBlockData *data = blockData(thisVal, api->blockClassId());
        if (data == nullptr)
            return JS_ThrowTypeError(ctx, "Block is not valid");

        return makeBlockType(ctx, blockIdentifierAt(api, data));
    }

    JSValue blockGetIsValid(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        return JS_NewBool(ctx, blockData(thisVal, api->blockClassId()) != nullptr);
    }

    JSValue blockMatches(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ScriptBlockData *data = blockData(thisVal, api->blockClassId());
        if (data == nullptr || argc < 1)
            return JS_NewBool(ctx, false);

        std::string target = toStdString(ctx, argv[0]);
        if (target.find(':') == std::string::npos)
            target = "minecraft:" + target;

        return JS_NewBool(ctx, blockIdentifierAt(api, data) == target);
    }

    JSValue blockGetLocation(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ScriptBlockData *data = blockData(thisVal, api->blockClassId());
        if (data == nullptr)
            return JS_ThrowTypeError(ctx, "Block is not valid");

        return makeVector3(ctx, (float) data->mX, (float) data->mY, (float) data->mZ);
    }

    JSValue blockGetDimension(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ScriptBlockData *data = blockData(thisVal, api->blockClassId());
        if (data == nullptr)
            return JS_ThrowTypeError(ctx, "Block is not valid");
        return api->makeDimension(data->mDimension);
    }

    JSValue blockCenter(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ScriptBlockData *data = blockData(thisVal, api->blockClassId());
        if (data == nullptr)
            return JS_ThrowTypeError(ctx, "Block is not valid");
        return makeVector3(ctx, (float) data->mX + 0.5f, (float) data->mY + 0.5f, (float) data->mZ + 0.5f);
    }

    JSValue blockOffset(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ScriptBlockData *data = blockData(thisVal, api->blockClassId());
        if (data == nullptr)
            return JS_ThrowTypeError(ctx, "Block is not valid");

        int32_t steps = 1;
        if (argc >= 1)
            JS_ToInt32(ctx, &steps, argv[0]);

        int32_t x = data->mX, y = data->mY, z = data->mZ;
        switch (magic) {
            case 0: y += steps; break;
            case 1: y -= steps; break;
            case 2: z -= steps; break;
            case 3: z += steps; break;
            case 4: x += steps; break;
            case 5: x -= steps; break;
            default: break;
        }
        return api->makeBlock(data->mDimension, x, y, z);
    }

    JSValue blockSetType(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ScriptBlockData *data = blockData(thisVal, api->blockClassId());
        if (data == nullptr)
            return JS_ThrowTypeError(ctx, "Block is not valid");

        if (argc < 1)
            return JS_ThrowTypeError(ctx, "setType requires a block identifier");

        std::string identifier;
        if (JS_IsObject(argv[0])) {
            JSValue typeValue = JS_GetPropertyStr(ctx, argv[0], "id");
            identifier = toStdString(ctx, typeValue);
            JS_FreeValue(ctx, typeValue);
        } else {
            identifier = toStdString(ctx, argv[0]);
        }

        api->setBlockType(data->mDimension, data->mX, data->mY, data->mZ, identifier);
        return JS_UNDEFINED;
    }

    JSValue blockSetPermutation(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ScriptBlockData *data = blockData(thisVal, api->blockClassId());
        if (data == nullptr)
            return JS_ThrowTypeError(ctx, "Block is not valid");

        if (argc < 1 || !JS_IsObject(argv[0]))
            return JS_ThrowTypeError(ctx, "setPermutation requires a BlockPermutation");

        JSValue typeValue = JS_GetPropertyStr(ctx, argv[0], "type");
        JSValue idValue = JS_GetPropertyStr(ctx, typeValue, "id");
        const std::string identifier = toStdString(ctx, idValue);
        JS_FreeValue(ctx, idValue);
        JS_FreeValue(ctx, typeValue);

        if (!identifier.empty())
            api->setBlockType(data->mDimension, data->mX, data->mY, data->mZ, identifier);
        return JS_UNDEFINED;
    }

}

void ScriptApi::_buildBlockClass() {
    JS_NewClassID(mRuntime, &mBlockClassId);

    JSClassDef blockClass{};
    blockClass.class_name = "Block";
    blockClass.finalizer = [](JSRuntime *, JSValue value) {};
    JS_NewClass(mRuntime, mBlockClassId, &blockClass);

    mBlockPrototype = JS_NewObject(mContext);

    JS_SetPropertyStr(mContext, mBlockPrototype, "setType",
                      JS_NewCFunction(mContext, blockSetType, "setType", 1));
    JS_SetPropertyStr(mContext, mBlockPrototype, "setPermutation",
                      JS_NewCFunction(mContext, blockSetPermutation, "setPermutation", 1));
    JS_SetPropertyStr(mContext, mBlockPrototype, "center",
                      JS_NewCFunction(mContext, blockCenter, "center", 0));
    JS_SetPropertyStr(mContext, mBlockPrototype, "above",
                      JS_NewCFunctionMagic(mContext, blockOffset, "above", 1, JS_CFUNC_generic_magic, 0));
    JS_SetPropertyStr(mContext, mBlockPrototype, "below",
                      JS_NewCFunctionMagic(mContext, blockOffset, "below", 1, JS_CFUNC_generic_magic, 1));
    JS_SetPropertyStr(mContext, mBlockPrototype, "north",
                      JS_NewCFunctionMagic(mContext, blockOffset, "north", 1, JS_CFUNC_generic_magic, 2));
    JS_SetPropertyStr(mContext, mBlockPrototype, "south",
                      JS_NewCFunctionMagic(mContext, blockOffset, "south", 1, JS_CFUNC_generic_magic, 3));
    JS_SetPropertyStr(mContext, mBlockPrototype, "east",
                      JS_NewCFunctionMagic(mContext, blockOffset, "east", 1, JS_CFUNC_generic_magic, 4));
    JS_SetPropertyStr(mContext, mBlockPrototype, "west",
                      JS_NewCFunctionMagic(mContext, blockOffset, "west", 1, JS_CFUNC_generic_magic, 5));
    JS_SetPropertyStr(mContext, mBlockPrototype, "matches",
                      JS_NewCFunction(mContext, blockMatches, "matches", 1));

    JSAtom locationAtom = JS_NewAtom(mContext, "location");
    JS_DefinePropertyGetSet(mContext, mBlockPrototype, locationAtom,
                            JS_NewCFunction(mContext, blockGetLocation, "get location", 0), JS_UNDEFINED,
                            JS_PROP_CONFIGURABLE);
    JS_FreeAtom(mContext, locationAtom);

    JSAtom dimensionAtom = JS_NewAtom(mContext, "dimension");
    JS_DefinePropertyGetSet(mContext, mBlockPrototype, dimensionAtom,
                            JS_NewCFunction(mContext, blockGetDimension, "get dimension", 0), JS_UNDEFINED,
                            JS_PROP_CONFIGURABLE);
    JS_FreeAtom(mContext, dimensionAtom);

    JSAtom validAtom = JS_NewAtom(mContext, "isValid");
    JS_DefinePropertyGetSet(mContext, mBlockPrototype, validAtom,
                            JS_NewCFunction(mContext, blockGetIsValid, "get isValid", 0), JS_UNDEFINED,
                            JS_PROP_CONFIGURABLE);
    JS_FreeAtom(mContext, validAtom);

    static const char *coordNames[] = {"x", "y", "z"};
    for (int i = 0; i < 3; ++i) {
        JSAtom atom = JS_NewAtom(mContext, coordNames[i]);
        JSValue getter = JS_NewCFunctionMagic(mContext, blockGetCoord, "get", 0, JS_CFUNC_generic_magic, i);
        JS_DefinePropertyGetSet(mContext, mBlockPrototype, atom, getter, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
        JS_FreeAtom(mContext, atom);
    }

    JSAtom typeAtom = JS_NewAtom(mContext, "typeId");
    JS_DefinePropertyGetSet(mContext, mBlockPrototype, typeAtom,
                            JS_NewCFunction(mContext, blockGetTypeId, "get typeId", 0), JS_UNDEFINED,
                            JS_PROP_CONFIGURABLE);
    JS_FreeAtom(mContext, typeAtom);

    JSAtom airAtom = JS_NewAtom(mContext, "isAir");
    JS_DefinePropertyGetSet(mContext, mBlockPrototype, airAtom,
                            JS_NewCFunction(mContext, blockGetIsAir, "get isAir", 0), JS_UNDEFINED,
                            JS_PROP_CONFIGURABLE);
    JS_FreeAtom(mContext, airAtom);

    JSAtom permAtom = JS_NewAtom(mContext, "permutation");
    JS_DefinePropertyGetSet(mContext, mBlockPrototype, permAtom,
                            JS_NewCFunction(mContext, blockGetPermutation, "get permutation", 0), JS_UNDEFINED,
                            JS_PROP_CONFIGURABLE);
    JS_FreeAtom(mContext, permAtom);

    JS_SetClassProto(mContext, mBlockClassId, JS_DupValue(mContext, mBlockPrototype));
}

JSValue ScriptApi::makeBlock(DimensionType dimension, int32_t x, int32_t y, int32_t z) {
    ScriptBlockData *data = new ScriptBlockData();
    data->mDimension = dimension;
    data->mX = x;
    data->mY = y;
    data->mZ = z;

    JSValue object = JS_NewObjectProtoClass(mContext, mBlockPrototype, mBlockClassId);
    JS_SetOpaque(object, data);
    return object;
}

// ========================= ItemStack =========================

namespace {
    JSValue itemStackGetTypeId(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ScriptItem *item = (ScriptItem *) JS_GetOpaque(thisVal, api->itemStackClassId());
        if (item == nullptr)
            return JS_ThrowTypeError(ctx, "ItemStack is not valid");

        return jsString(ctx, item->mTypeId);
    }

    JSValue itemStackGetAmount(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ScriptItem *item = (ScriptItem *) JS_GetOpaque(thisVal, api->itemStackClassId());
        if (item == nullptr)
            return JS_ThrowTypeError(ctx, "ItemStack is not valid");

        return JS_NewInt32(ctx, item->mAmount);
    }

    JSValue itemStackSetAmount(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ScriptItem *item = (ScriptItem *) JS_GetOpaque(thisVal, api->itemStackClassId());
        if (item == nullptr)
            return JS_ThrowTypeError(ctx, "ItemStack is not valid");

        int32_t amount = 1;
        if (argc >= 1)
            JS_ToInt32(ctx, &amount, argv[0]);
        item->mAmount = amount;
        return JS_UNDEFINED;
    }

    JSValue itemStackGetDynamicProperty(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ScriptItem *item = (ScriptItem *) JS_GetOpaque(thisVal, api->itemStackClassId());
        if (item == nullptr || argc < 1)
            return JS_UNDEFINED;
        return dynamicPropertyGet(ctx, item->mDynamicProperties, toStdString(ctx, argv[0]));
    }

    JSValue itemStackSetDynamicProperty(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ScriptItem *item = (ScriptItem *) JS_GetOpaque(thisVal, api->itemStackClassId());
        if (item == nullptr || argc < 1)
            return JS_UNDEFINED;
        dynamicPropertySet(ctx, item->mDynamicProperties, toStdString(ctx, argv[0]),
                           argc >= 2 ? argv[1] : JS_UNDEFINED);
        return JS_UNDEFINED;
    }

    JSValue itemStackGetDynamicPropertyIds(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ScriptItem *item = (ScriptItem *) JS_GetOpaque(thisVal, api->itemStackClassId());
        if (item == nullptr)
            return JS_NewArray(ctx);
        return dynamicPropertyIds(ctx, item->mDynamicProperties);
    }

    int32_t itemMaxDurability(const std::string &typeId) {
        for (const CustomItemDefinition &definition: CustomContentRegistry::getInstance().getItems()) {
            if (definition.mIdentifier == typeId)
                return definition.mMaxDurability;
        }
        return 0;
    }

    JSValue durabilityGetDamage(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        JSValue itemValue = JS_GetPropertyStr(ctx, thisVal, "_item");
        ScriptItem *item = (ScriptItem *) JS_GetOpaque(itemValue, api->itemStackClassId());
        JS_FreeValue(ctx, itemValue);
        return JS_NewInt32(ctx, item == nullptr ? 0 : item->mDamage);
    }

    JSValue durabilitySetDamage(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        JSValue itemValue = JS_GetPropertyStr(ctx, thisVal, "_item");
        ScriptItem *item = (ScriptItem *) JS_GetOpaque(itemValue, api->itemStackClassId());
        JS_FreeValue(ctx, itemValue);
        if (item != nullptr && argc >= 1) {
            int32_t value = 0;
            JS_ToInt32(ctx, &value, argv[0]);
            item->mDamage = value < 0 ? 0 : value;
        }
        return JS_UNDEFINED;
    }

    JSValue durabilityGetDamageChance(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        int32_t unbreaking = 0;
        if (argc >= 1)
            JS_ToInt32(ctx, &unbreaking, argv[0]);
        if (unbreaking < 0)
            unbreaking = 0;
        return JS_NewInt32(ctx, (int32_t) (100.0 / (double) (unbreaking + 1) + 0.5));
    }

    JSValue enchantableGetEnchantment(JSContext *, JSValueConst, int, JSValueConst *) {
        return JS_UNDEFINED;
    }

    JSValue itemStackGetComponent(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ScriptItem *item = (ScriptItem *) JS_GetOpaque(thisVal, api->itemStackClassId());
        if (item == nullptr || argc < 1)
            return JS_UNDEFINED;

        const std::string id = toStdString(ctx, argv[0]);

        if (id == "durability" || id == "minecraft:durability") {
            const int32_t maxDurability = itemMaxDurability(item->mTypeId);
            if (maxDurability <= 0)
                return JS_UNDEFINED;

            JSValue component = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, component, "_item", JS_DupValue(ctx, thisVal));
            JS_SetPropertyStr(ctx, component, "maxDurability", JS_NewInt32(ctx, maxDurability));
            JS_SetPropertyStr(ctx, component, "getDamageChance",
                              JS_NewCFunction(ctx, durabilityGetDamageChance, "getDamageChance", 1));

            JSAtom damageAtom = JS_NewAtom(ctx, "damage");
            JS_DefinePropertyGetSet(ctx, component, damageAtom,
                                    JS_NewCFunction(ctx, durabilityGetDamage, "get damage", 0),
                                    JS_NewCFunction(ctx, durabilitySetDamage, "set damage", 1),
                                    JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, damageAtom);
            return component;
        }

        if (id == "enchantable" || id == "minecraft:enchantable") {
            JSValue component = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, component, "getEnchantment",
                              JS_NewCFunction(ctx, enchantableGetEnchantment, "getEnchantment", 1));
            return component;
        }

        if (id != "cooldown" && id != "minecraft:cooldown")
            return JS_UNDEFINED;

        int32_t cooldownTicks = 0;
        for (const CustomItemDefinition &definition: CustomContentRegistry::getInstance().getItems()) {
            if (definition.mIdentifier == item->mTypeId) {
                cooldownTicks = definition.mCooldownTicks;
                break;
            }
        }

        JSValue component = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, component, "cooldownTicks", JS_NewInt32(ctx, cooldownTicks));
        JS_SetPropertyStr(ctx, component, "cooldownCategory", JS_NewString(ctx, "item"));
        return component;
    }

    JSValue itemStackConstructor(JSContext *ctx, JSValueConst newTarget, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (argc < 1)
            return JS_ThrowTypeError(ctx, "ItemStack requires an item identifier");

        std::string typeId;
        if (JS_IsObject(argv[0])) {
            JSValue idValue = JS_GetPropertyStr(ctx, argv[0], "id");
            typeId = toStdString(ctx, idValue);
            JS_FreeValue(ctx, idValue);
        } else {
            typeId = toStdString(ctx, argv[0]);
        }

        if (typeId.empty())
            return JS_ThrowTypeError(ctx, "ItemStack requires an item identifier");
        if (typeId.find(':') == std::string::npos)
            typeId = "minecraft:" + typeId;

        int32_t amount = 1;
        if (argc >= 2)
            JS_ToInt32(ctx, &amount, argv[1]);

        return api->makeItemStack(typeId, amount < 1 ? 1 : amount);
    }

    JSValue itemStackGetTags(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ScriptItem *item = (ScriptItem *) JS_GetOpaque(thisVal, api->itemStackClassId());
        if (item == nullptr)
            return JS_ThrowTypeError(ctx, "ItemStack is not valid");

        JSValue tags = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const std::string &tag: CraftingRecipeTable::getItemTags(item->mTypeId))
            JS_SetPropertyUint32(ctx, tags, index++, jsString(ctx, tag));
        return tags;
    }

    JSValue itemStackHasTag(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        ScriptItem *item = (ScriptItem *) JS_GetOpaque(thisVal, api->itemStackClassId());
        if (item == nullptr)
            return JS_ThrowTypeError(ctx, "ItemStack is not valid");
        if (argc < 1)
            return JS_FALSE;

        const std::string wanted = toStdString(ctx, argv[0]);
        for (const std::string &tag: CraftingRecipeTable::getItemTags(item->mTypeId)) {
            if (tag == wanted)
                return JS_TRUE;
        }
        return JS_FALSE;
    }
}

void ScriptApi::_buildItemStackClass() {
    JS_NewClassID(mRuntime, &mItemStackClassId);

    JSClassDef itemStackClass{};
    itemStackClass.class_name = "ItemStack";
    itemStackClass.finalizer = [](JSRuntime *runtime, JSValue value) {
        ScriptApi *api = ScriptApi::fromRuntime(runtime);
        if (api == nullptr)
            return;
        ScriptItem *item = (ScriptItem *) JS_GetOpaque(value, api->itemStackClassId());
        delete item;
    };
    JS_NewClass(mRuntime, mItemStackClassId, &itemStackClass);

    mItemStackPrototype = JS_NewObject(mContext);

    JSAtom typeAtom = JS_NewAtom(mContext, "typeId");
    JS_DefinePropertyGetSet(mContext, mItemStackPrototype, typeAtom,
                            JS_NewCFunction(mContext, itemStackGetTypeId, "get typeId", 0), JS_UNDEFINED,
                            JS_PROP_CONFIGURABLE);
    JS_FreeAtom(mContext, typeAtom);

    JSAtom amountAtom = JS_NewAtom(mContext, "amount");
    JS_DefinePropertyGetSet(mContext, mItemStackPrototype, amountAtom,
                            JS_NewCFunction(mContext, itemStackGetAmount, "get amount", 0),
                            JS_NewCFunction(mContext, itemStackSetAmount, "set amount", 1),
                            JS_PROP_CONFIGURABLE);
    JS_FreeAtom(mContext, amountAtom);

    JS_SetPropertyStr(mContext, mItemStackPrototype, "getDynamicProperty",
                      JS_NewCFunction(mContext, itemStackGetDynamicProperty, "getDynamicProperty", 1));
    JS_SetPropertyStr(mContext, mItemStackPrototype, "setDynamicProperty",
                      JS_NewCFunction(mContext, itemStackSetDynamicProperty, "setDynamicProperty", 2));
    JS_SetPropertyStr(mContext, mItemStackPrototype, "getDynamicPropertyIds",
                      JS_NewCFunction(mContext, itemStackGetDynamicPropertyIds, "getDynamicPropertyIds", 0));
    JS_SetPropertyStr(mContext, mItemStackPrototype, "getComponent",
                      JS_NewCFunction(mContext, itemStackGetComponent, "getComponent", 1));
    JS_SetPropertyStr(mContext, mItemStackPrototype, "getTags",
                      JS_NewCFunction(mContext, itemStackGetTags, "getTags", 0));
    JS_SetPropertyStr(mContext, mItemStackPrototype, "hasTag",
                      JS_NewCFunction(mContext, itemStackHasTag, "hasTag", 1));

    JS_SetClassProto(mContext, mItemStackClassId, JS_DupValue(mContext, mItemStackPrototype));
}

JSValue ScriptApi::makeItemStack(const std::string &typeId, int32_t amount) {
    ScriptItem *item = new ScriptItem();
    item->mTypeId = typeId;
    item->mAmount = amount;

    JSValue object = JS_NewObjectProtoClass(mContext, mItemStackPrototype, mItemStackClassId);
    JS_SetOpaque(object, item);
    return object;
}

JSValue ScriptApi::makeHeldItemStack(ServerPlayer &player, const std::string &identifier) {
    const ItemStack &stack = player.getInventory().getItemInHand();

    std::string typeId = identifier;
    int32_t count = 1;
    const Tag *dynamicProperties = nullptr;
    if (!stack.isAir() && stack.mDefinition != nullptr) {
        typeId = stack.mDefinition->getIdentifier();
        count = stack.mCount;
        if (stack.mTag.isCompound())
            dynamicProperties = stack.mTag.get("DynamicProperties");
    }

    JSValue jsItem = makeItemStack(typeId, count);
    if (dynamicProperties != nullptr) {
        ScriptItem *item = (ScriptItem *) JS_GetOpaque(jsItem, mItemStackClassId);
        if (item != nullptr)
            deserializeDynamicProperties(*dynamicProperties, item->mDynamicProperties);
    }
    return jsItem;
}

// ========================= Dimension =========================

namespace {
    JSValue dimensionGetBlock(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));

        double x = 0.0, y = 0.0, z = 0.0;
        if (argc < 1 || !readVector3(ctx, argv[0], x, y, z))
            return JS_ThrowTypeError(ctx, "getBlock requires a location {x,y,z}");

        return api->makeBlock(ScriptApi::dimensionOf(ctx, thisVal), (int32_t) std::floor(x), (int32_t) std::floor(y),
                              (int32_t) std::floor(z));
    }

    JSValue dimensionSetBlockType(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (argc < 2)
            return JS_UNDEFINED;

        double x = 0.0, y = 0.0, z = 0.0;
        if (!readVector3(ctx, argv[0], x, y, z))
            return JS_ThrowTypeError(ctx, "setBlockType requires a location {x,y,z}");

        std::string identifier;
        if (JS_IsObject(argv[1])) {
            JSValue typeValue = JS_GetPropertyStr(ctx, argv[1], "id");
            identifier = toStdString(ctx, typeValue);
            JS_FreeValue(ctx, typeValue);
        } else {
            identifier = toStdString(ctx, argv[1]);
        }

        api->setBlockType(ScriptApi::dimensionOf(ctx, thisVal), (int32_t) std::floor(x), (int32_t) std::floor(y),
                          (int32_t) std::floor(z), identifier);
        return JS_UNDEFINED;
    }

    JSValue dimensionRunCommand(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));

        const std::string line = argc >= 1 ? toStdString(ctx, argv[0]) : std::string();
        const int32_t success = api->runCommandAsConsole(line);

        JSValue result = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, result, "successCount", JS_NewInt32(ctx, success));
        return result;
    }

    JSValue dimensionGetPlayers(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        const DimensionType dimension = ScriptApi::dimensionOf(ctx, thisVal);

        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (auto &entry: api->host().getPlayers()) {
            if (!entry.second.isSpawned() || entry.second.getDimension() != dimension)
                continue;

            JS_SetPropertyUint32(ctx, array, index, api->makePlayer(entry.second));
            index++;
        }
        return array;
    }

    JSValue dimensionSpawnEntity(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (argc < 2)
            return JS_ThrowTypeError(ctx, "spawnEntity requires a type and a location");

        const std::string typeId = toStdString(ctx, argv[0]);
        double x = 0.0, y = 0.0, z = 0.0;
        if (!readVector3(ctx, argv[1], x, y, z))
            return JS_ThrowTypeError(ctx, "spawnEntity requires a location {x,y,z}");

        ServerActor *actor = api->host().spawnActor(api->host().getDimension(ScriptApi::dimensionOf(ctx, thisVal)),
                                                    typeId, Vector3f((float) x, (float) y, (float) z));
        if (actor == nullptr)
            return JS_UNDEFINED;
        return api->makeActor(*actor);
    }

    JSValue dimensionSpawnItem(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (argc < 2)
            return JS_UNDEFINED;

        std::string typeId;
        int32_t amount = 1;
        if (!readItemFromJs(ctx, api->itemStackClassId(), argv[0], typeId, amount))
            return JS_UNDEFINED;

        double x = 0.0, y = 0.0, z = 0.0;
        if (!readVector3(ctx, argv[1], x, y, z))
            return JS_UNDEFINED;

        api->host().spawnItemActor(api->host().getDimension(ScriptApi::dimensionOf(ctx, thisVal)), typeId, amount,
                                   Vector3f((float) x, (float) y, (float) z));
        return JS_UNDEFINED;
    }

    JSValue dimensionSpawnParticle(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (argc < 2)
            return JS_UNDEFINED;

        const std::string identifier = toStdString(ctx, argv[0]);
        double x = 0.0, y = 0.0, z = 0.0;
        if (!readVector3(ctx, argv[1], x, y, z))
            return JS_UNDEFINED;

        api->host().spawnParticleEffect(api->host().getDimension(ScriptApi::dimensionOf(ctx, thisVal)), identifier,
                                        Vector3f((float) x, (float) y, (float) z));
        return JS_UNDEFINED;
    }

    JSValue dimensionPlaySound(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (argc < 2)
            return JS_UNDEFINED;

        const std::string sound = toStdString(ctx, argv[0]);
        double x = 0.0, y = 0.0, z = 0.0;
        if (!readVector3(ctx, argv[1], x, y, z))
            return JS_UNDEFINED;

        float volume = 1.0f;
        float pitch = 1.0f;
        if (argc >= 3 && JS_IsObject(argv[2])) {
            JSValue volumeValue = JS_GetPropertyStr(ctx, argv[2], "volume");
            JSValue pitchValue = JS_GetPropertyStr(ctx, argv[2], "pitch");
            double parsed = 1.0;
            if (JS_ToFloat64(ctx, &parsed, volumeValue) == 0)
                volume = (float) parsed;
            if (JS_ToFloat64(ctx, &parsed, pitchValue) == 0)
                pitch = (float) parsed;
            JS_FreeValue(ctx, volumeValue);
            JS_FreeValue(ctx, pitchValue);
        }

        api->host().playNamedSound(api->host().getDimension(ScriptApi::dimensionOf(ctx, thisVal)), sound,
                                   Vector3f((float) x, (float) y, (float) z), volume, pitch);
        return JS_UNDEFINED;
    }

    JSValue dimensionGetEntities(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        const DimensionType dimension = ScriptApi::dimensionOf(ctx, thisVal);

        double centerX = 0.0, centerY = 0.0, centerZ = 0.0;
        double maxDistance = -1.0;
        const bool hasOptions = argc >= 1 && JS_IsObject(argv[0]);
        if (hasOptions) {
            JSValue location = JS_GetPropertyStr(ctx, argv[0], "location");
            if (JS_IsObject(location))
                readVector3(ctx, location, centerX, centerY, centerZ);
            JS_FreeValue(ctx, location);

            JSValue maxDistanceValue = JS_GetPropertyStr(ctx, argv[0], "maxDistance");
            if (!JS_IsUndefined(maxDistanceValue))
                JS_ToFloat64(ctx, &maxDistance, maxDistanceValue);
            JS_FreeValue(ctx, maxDistanceValue);
        }

        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (auto &entry: api->host().getActors()) {
            ServerActor &actor = *entry.second;
            if (!actor.isAlive() || actor.getDimension() != dimension)
                continue;

            if (hasOptions && !actorMatchesQuery(ctx, actor, argv[0]))
                continue;

            if (maxDistance >= 0.0) {
                const Vector3f &position = actor.getPosition();
                const double dx = position.x - centerX;
                const double dy = position.y - centerY;
                const double dz = position.z - centerZ;
                if (dx * dx + dy * dy + dz * dz > maxDistance * maxDistance)
                    continue;
            }

            JS_SetPropertyUint32(ctx, array, index++, api->makeActor(actor));
        }

        bool typeAllowsPlayer = true;
        bool familiesRequired = false;
        bool tagsRequired = false;
        if (hasOptions) {
            JSValue typeValue = JS_GetPropertyStr(ctx, argv[0], "type");
            if (JS_IsString(typeValue)) {
                const std::string type = toStdString(ctx, typeValue);
                typeAllowsPlayer = type.empty() || type == "minecraft:player";
            }
            JS_FreeValue(ctx, typeValue);

            JSValue excludeTypes = JS_GetPropertyStr(ctx, argv[0], "excludeTypes");
            if (arrayContainsString(ctx, excludeTypes, "minecraft:player"))
                typeAllowsPlayer = false;
            JS_FreeValue(ctx, excludeTypes);

            JSValue families = JS_GetPropertyStr(ctx, argv[0], "families");
            familiesRequired = JS_IsArray(families);
            JS_FreeValue(ctx, families);

            JSValue tags = JS_GetPropertyStr(ctx, argv[0], "tags");
            tagsRequired = JS_IsArray(tags);
            JS_FreeValue(ctx, tags);
        }

        if (typeAllowsPlayer && !familiesRequired) {
            JSValue requiredTags = tagsRequired ? JS_GetPropertyStr(ctx, argv[0], "tags") : JS_UNDEFINED;
            uint32_t requiredTagCount = 0;
            if (tagsRequired) {
                JSValue lengthValue = JS_GetPropertyStr(ctx, requiredTags, "length");
                JS_ToUint32(ctx, &requiredTagCount, lengthValue);
                JS_FreeValue(ctx, lengthValue);
            }

            for (auto &entry: api->host().getPlayers()) {
                ServerPlayer &player = entry.second;
                if (!player.isSpawned() || player.getDimension() != dimension)
                    continue;

                if (maxDistance >= 0.0) {
                    const Vector3f &position = player.getPosition();
                    const double dx = position.x - centerX;
                    const double dy = position.y - centerY;
                    const double dz = position.z - centerZ;
                    if (dx * dx + dy * dy + dz * dz > maxDistance * maxDistance)
                        continue;
                }

                if (tagsRequired) {
                    bool hasAllTags = true;
                    for (uint32_t i = 0; i < requiredTagCount; ++i) {
                        JSValue element = JS_GetPropertyUint32(ctx, requiredTags, i);
                        const std::string tag = toStdString(ctx, element);
                        JS_FreeValue(ctx, element);
                        if (!player.hasTag(tag)) {
                            hasAllTags = false;
                            break;
                        }
                    }
                    if (!hasAllTags)
                        continue;
                }

                JS_SetPropertyUint32(ctx, array, index++, api->makePlayer(player));
            }

            if (tagsRequired)
                JS_FreeValue(ctx, requiredTags);
        }

        return array;
    }
}

DimensionType ScriptApi::dimensionOf(JSContext *ctx, JSValueConst dimension) {
    JSValue idValue = JS_GetPropertyStr(ctx, dimension, "id");
    const std::string identifier = toStdString(ctx, idValue);
    JS_FreeValue(ctx, idValue);

    if (identifier == "minecraft:nether")
        return DimensionType::Nether;
    if (identifier == "minecraft:the_end")
        return DimensionType::TheEnd;
    return DimensionType::Overworld;
}

JSValue ScriptApi::makeDimension(DimensionType dimensionType) {
    JSValue dimension = JS_NewObject(mContext);

    const char *identifier = dimensionType == DimensionType::Nether
                             ? "minecraft:nether"
                             : dimensionType == DimensionType::TheEnd ? "minecraft:the_end" : "minecraft:overworld";
    JS_SetPropertyStr(mContext, dimension, "id", JS_NewString(mContext, identifier));
    JS_SetPropertyStr(mContext, dimension, "setBlockType",
                      JS_NewCFunction(mContext, dimensionSetBlockType, "setBlockType", 2));
    JS_SetPropertyStr(mContext, dimension, "getBlock",
                      JS_NewCFunction(mContext, dimensionGetBlock, "getBlock", 1));
    JS_SetPropertyStr(mContext, dimension, "runCommand",
                      JS_NewCFunction(mContext, dimensionRunCommand, "runCommand", 1));
    JS_SetPropertyStr(mContext, dimension, "getPlayers",
                      JS_NewCFunction(mContext, dimensionGetPlayers, "getPlayers", 0));
    JS_SetPropertyStr(mContext, dimension, "spawnEntity",
                      JS_NewCFunction(mContext, dimensionSpawnEntity, "spawnEntity", 2));
    JS_SetPropertyStr(mContext, dimension, "spawnItem",
                      JS_NewCFunction(mContext, dimensionSpawnItem, "spawnItem", 2));
    JS_SetPropertyStr(mContext, dimension, "spawnParticle",
                      JS_NewCFunction(mContext, dimensionSpawnParticle, "spawnParticle", 2));
    JS_SetPropertyStr(mContext, dimension, "playSound",
                      JS_NewCFunction(mContext, dimensionPlaySound, "playSound", 3));
    JS_SetPropertyStr(mContext, dimension, "getEntities",
                      JS_NewCFunction(mContext, dimensionGetEntities, "getEntities", 1));
    return dimension;
}

// ========================= Server UI =========================

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

    if (!cancelled && !response.empty()) {
        JSValue parsed = JS_ParseJSON(mContext, response.c_str(), response.size(), "<form-response>");
        if (JS_IsException(parsed)) {
            JS_FreeValue(mContext, JS_GetException(mContext));
            JS_SetPropertyStr(mContext, result, "value", JS_NULL);
        } else {
            JS_SetPropertyStr(mContext, result, "value", parsed);
        }
    } else {
        JS_SetPropertyStr(mContext, result, "value", JS_NULL);
    }

    JSValue callResult = JS_Call(mContext, resolve, JS_UNDEFINED, 1, &result);
    JS_FreeValue(mContext, callResult);
    JS_FreeValue(mContext, result);
    JS_FreeValue(mContext, resolve);
}

namespace {
    JSValue uiShowFormGlobal(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (argc < 2)
            return JS_ThrowTypeError(ctx, "showForm requires a player and form data");

        const std::string formData = toStdString(ctx, argv[1]);
        return api->showForm(argv[0], formData);
    }

    JSValue scoreboardSetScoreGlobal(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (argc < 3)
            return JS_UNDEFINED;
        int32_t score = 0;
        JS_ToInt32(ctx, &score, argv[2]);
        api->host().setScoreboardScore(toStdString(ctx, argv[0]), toStdString(ctx, argv[1]), score);
        return JS_UNDEFINED;
    }

    JSValue scoreboardRemoveScoreGlobal(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (argc < 2)
            return JS_UNDEFINED;
        api->host().removeScoreboardScore(toStdString(ctx, argv[0]), toStdString(ctx, argv[1]));
        return JS_UNDEFINED;
    }

    JSValue scoreboardDisplayGlobal(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (argc < 2)
            return JS_UNDEFINED;
        const std::string displayName = argc >= 3 ? toStdString(ctx, argv[2]) : std::string();
        api->host().displayScoreboardObjective(toStdString(ctx, argv[0]), toStdString(ctx, argv[1]), displayName);
        return JS_UNDEFINED;
    }

    JSValue scoreboardClearDisplayGlobal(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (argc < 1)
            return JS_UNDEFINED;
        api->host().clearScoreboardDisplay(toStdString(ctx, argv[0]));
        return JS_UNDEFINED;
    }

    JSValue scoreboardRemoveObjectiveGlobal(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (argc < 1)
            return JS_UNDEFINED;
        api->host().removeScoreboardObjective(toStdString(ctx, argv[0]));
        return JS_UNDEFINED;
    }

    const char *UI_BOOTSTRAP = R"JS(
(function () {
    const ft = (v) => (v !== null && typeof v === "object") ? (v.rawtext ? v : { rawtext: [v] }) : String(v);

    class ActionFormData {
        constructor() {
            this._data = { type: "form", title: "", content: "", buttons: [] };
        }
        title(text) { this._data.title = ft(text); return this; }
        body(text) { this._data.content = ft(text); return this; }
        button(text, icon) {
            const entry = { text: ft(text) };
            if (icon !== undefined) entry.image = { type: "path", data: String(icon) };
            this._data.buttons.push(entry);
            return this;
        }
        header(text) { return this; }
        label(text) { return this; }
        divider() { return this; }
        async show(player) {
            const r = await globalThis.__falconShowForm(player, JSON.stringify(this._data));
            if (r.canceled || r.value === null) return { canceled: true, cancelationReason: "UserClosed" };
            return { canceled: false, selection: r.value };
        }
    }

    class MessageFormData {
        constructor() {
            this._data = { type: "modal", title: "", content: "", button1: "", button2: "" };
        }
        title(text) { this._data.title = ft(text); return this; }
        body(text) { this._data.content = ft(text); return this; }
        button1(text) { this._data.button1 = ft(text); return this; }
        button2(text) { this._data.button2 = ft(text); return this; }
        async show(player) {
            const r = await globalThis.__falconShowForm(player, JSON.stringify(this._data));
            if (r.canceled || r.value === null) return { canceled: true, cancelationReason: "UserClosed" };
            return { canceled: false, selection: r.value === true ? 0 : 1 };
        }
    }

    class ModalFormData {
        constructor() {
            this._data = { type: "custom_form", title: "", content: [] };
        }
        title(text) { this._data.title = ft(text); return this; }
        textField(label, placeholder, defaultValue) {
            this._data.content.push({ type: "input", text: ft(label), placeholder: String(placeholder || ""), default: defaultValue === undefined ? "" : String(defaultValue) });
            return this;
        }
        toggle(label, defaultValue) {
            const options = (defaultValue !== null && typeof defaultValue === "object") ? defaultValue : { defaultValue };
            this._data.content.push({ type: "toggle", text: ft(label), default: options.defaultValue === true });
            return this;
        }
        slider(label, min, max, step, defaultValue) {
            let valueStep = 1;
            let def = Number(min);
            if (step !== null && typeof step === "object") {
                if (step.valueStep !== undefined) valueStep = Number(step.valueStep);
                if (step.defaultValue !== undefined) def = Number(step.defaultValue);
            } else {
                if (step !== undefined) valueStep = Number(step);
                if (defaultValue !== undefined) def = Number(defaultValue);
            }
            this._data.content.push({ type: "slider", text: ft(label), min: Number(min), max: Number(max), step: valueStep, default: def });
            return this;
        }
        dropdown(label, options, defaultIndex) {
            let def = 0;
            if (defaultIndex !== null && typeof defaultIndex === "object") {
                if (defaultIndex.defaultValueIndex !== undefined) def = Number(defaultIndex.defaultValueIndex);
            } else if (defaultIndex !== undefined) {
                def = Number(defaultIndex);
            }
            this._data.content.push({ type: "dropdown", text: ft(label), options: options.map(ft), default: def });
            return this;
        }
        label(text) { this._data.content.push({ type: "label", text: ft(text) }); return this; }
        header(text) { this._data.content.push({ type: "header", text: ft(text) }); return this; }
        divider() { this._data.content.push({ type: "divider" }); return this; }
        submitButton(text) { this._data.submit = ft(text); return this; }
        async show(player) {
            const r = await globalThis.__falconShowForm(player, JSON.stringify(this._data));
            if (r.canceled || r.value === null) return { canceled: true, cancelationReason: "UserClosed" };
            return { canceled: false, formValues: r.value };
        }
    }

    globalThis.__falconUI = { ActionFormData, MessageFormData, ModalFormData };

    function participantKey(participant) {
        if (participant === null || participant === undefined) return "";
        if (typeof participant === "string") return participant;
        if (participant.id !== undefined) return String(participant.id);
        if (participant.displayName !== undefined) return String(participant.displayName);
        return String(participant);
    }

    class ScoreboardObjective {
        constructor(id, displayName) {
            this.id = id;
            this.displayName = displayName || id;
            this._scores = new Map();
        }
        getScore(participant) {
            const key = participantKey(participant);
            return this._scores.has(key) ? this._scores.get(key) : undefined;
        }
        setScore(participant, value) {
            const key = participantKey(participant);
            this._scores.set(key, value | 0);
            globalThis.__falconScoreboardSetScore(this.id, key, value | 0);
        }
        addScore(participant, value) {
            const key = participantKey(participant);
            const current = this._scores.has(key) ? this._scores.get(key) : 0;
            const next = current + (value | 0);
            this._scores.set(key, next);
            globalThis.__falconScoreboardSetScore(this.id, key, next);
            return next;
        }
        removeParticipant(participant) {
            const key = participantKey(participant);
            globalThis.__falconScoreboardRemoveScore(this.id, key);
            return this._scores.delete(key);
        }
        hasParticipant(participant) {
            return this._scores.has(participantKey(participant));
        }
        getParticipants() {
            return Array.from(this._scores.keys()).map((id) => ({ displayName: id, id }));
        }
        getScores() {
            return Array.from(this._scores.entries()).map(([id, score]) => ({ participant: { displayName: id, id }, score }));
        }
    }

    globalThis.__falconMakeScoreboard = function () {
        const objectives = new Map();
        const displaySlots = new Map();
        return {
            addObjective(id, displayName) {
                if (objectives.has(id)) return objectives.get(id);
                const objective = new ScoreboardObjective(id, displayName);
                objectives.set(id, objective);
                return objective;
            },
            getObjective(id) {
                return objectives.has(id) ? objectives.get(id) : undefined;
            },
            removeObjective(id) {
                const key = typeof id === "string" ? id : id.id;
                globalThis.__falconScoreboardRemoveObjective(key);
                return objectives.delete(key);
            },
            getObjectives() {
                return Array.from(objectives.values());
            },
            getParticipants() {
                const set = new Map();
                for (const objective of objectives.values())
                    for (const participant of objective.getParticipants())
                        set.set(participant.id, participant);
                return Array.from(set.values());
            },
            setObjectiveAtDisplaySlot(slot, options) {
                const objective = options && options.objective;
                if (!objective) return undefined;
                globalThis.__falconScoreboardDisplay(slot, objective.id, objective.displayName || objective.id);
                displaySlots.set(slot, objective);
                return objective;
            },
            clearObjectiveAtDisplaySlot(slot) {
                const previous = displaySlots.get(slot);
                globalThis.__falconScoreboardClearDisplay(slot);
                displaySlots.delete(slot);
                return previous;
            },
            getObjectiveAtDisplaySlot(slot) {
                const objective = displaySlots.get(slot);
                return objective ? { objective, sortOrder: 0 } : undefined;
            }
        };
    };

    globalThis.__falconMakeTickingAreaManager = function () {
        const areas = new Map();
        return {
            createTickingArea(name, options) {
                areas.set(name, options || {});
                return { name };
            },
            removeTickingArea(name) {
                return areas.delete(name);
            },
            getTickingAreas() {
                return Array.from(areas.keys()).map((name) => ({ name }));
            }
        };
    };
})();

globalThis.__falconItemComponentHandlers = {};
globalThis.__falconBlockComponentHandlers = {};

globalThis.__falconItemComponentRegistry = {
    registerCustomComponent(id, handlers) {
        globalThis.__falconItemComponentHandlers[id] = handlers;
    }
};

globalThis.__falconBlockComponentRegistry = {
    registerCustomComponent(id, handlers) {
        globalThis.__falconBlockComponentHandlers[id] = handlers;
    }
};

globalThis.__falconFireItemComponents = function (hook, ids, event) {
    if (!ids) return;
    for (const id of ids) {
        const handler = globalThis.__falconItemComponentHandlers[id];
        if (handler && typeof handler[hook] === "function") {
            try {
                handler[hook](event);
            } catch (error) {
                console.warn("custom component " + id + "." + hook + ": " + error);
            }
        }
    }
};
)JS";

    int uiModuleInit(JSContext *ctx, JSModuleDef *module) {
        JSValue global = JS_GetGlobalObject(ctx);
        JSValue registry = JS_GetPropertyStr(ctx, global, "__falconUI");

        JSValue actionForm = JS_GetPropertyStr(ctx, registry, "ActionFormData");
        JSValue messageForm = JS_GetPropertyStr(ctx, registry, "MessageFormData");
        JSValue modalForm = JS_GetPropertyStr(ctx, registry, "ModalFormData");

        JS_SetModuleExport(ctx, module, "ActionFormData", actionForm);
        JS_SetModuleExport(ctx, module, "MessageFormData", messageForm);
        JS_SetModuleExport(ctx, module, "ModalFormData", modalForm);

        JS_FreeValue(ctx, registry);
        JS_FreeValue(ctx, global);
        return 0;
    }
}

void ScriptApi::_installUiBootstrap() {
    JSValue global = JS_GetGlobalObject(mContext);
    JS_SetPropertyStr(mContext, global, "__falconShowForm",
                      JS_NewCFunction(mContext, uiShowFormGlobal, "__falconShowForm", 2));
    JS_SetPropertyStr(mContext, global, "__falconScoreboardSetScore",
                      JS_NewCFunction(mContext, scoreboardSetScoreGlobal, "__falconScoreboardSetScore", 3));
    JS_SetPropertyStr(mContext, global, "__falconScoreboardRemoveScore",
                      JS_NewCFunction(mContext, scoreboardRemoveScoreGlobal, "__falconScoreboardRemoveScore", 2));
    JS_SetPropertyStr(mContext, global, "__falconScoreboardDisplay",
                      JS_NewCFunction(mContext, scoreboardDisplayGlobal, "__falconScoreboardDisplay", 3));
    JS_SetPropertyStr(mContext, global, "__falconScoreboardClearDisplay",
                      JS_NewCFunction(mContext, scoreboardClearDisplayGlobal, "__falconScoreboardClearDisplay", 1));
    JS_SetPropertyStr(mContext, global, "__falconScoreboardRemoveObjective",
                      JS_NewCFunction(mContext, scoreboardRemoveObjectiveGlobal, "__falconScoreboardRemoveObjective", 1));
    JS_FreeValue(mContext, global);

    JSValue result = JS_Eval(mContext, UI_BOOTSTRAP, strlen(UI_BOOTSTRAP), "<falcon-ui>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(mContext);
        const char *message = JS_ToCString(mContext, exception);
        LOG_ERROR(LogAreaID::Server, "UI bootstrap error: %s", message == nullptr ? "?" : message);
        if (message != nullptr)
            JS_FreeCString(mContext, message);
        JS_FreeValue(mContext, exception);
    }
    JS_FreeValue(mContext, result);
}

void ScriptApi::_registerUiModule() {
    JSModuleDef *module = JS_NewCModule(mContext, "@minecraft/server-ui", uiModuleInit);
    if (module == nullptr)
        return;

    JS_AddModuleExport(mContext, module, "ActionFormData");
    JS_AddModuleExport(mContext, module, "MessageFormData");
    JS_AddModuleExport(mContext, module, "ModalFormData");
}

// ========================= Module + lifecycle =========================

namespace {
    JSValue stateValueToJs(JSContext *ctx, const Tag &value) {
        switch (value.getType()) {
            case Tag::Type::Byte:
                return JS_NewBool(ctx, value.asByte() != 0);
            case Tag::Type::Short:
                return JS_NewInt32(ctx, value.asShort());
            case Tag::Type::Int:
                return JS_NewInt32(ctx, value.asInt());
            case Tag::Type::String:
                return jsString(ctx, value.asString());
            default:
                return JS_UNDEFINED;
        }
    }

    JSValue paletteBlockNames(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        JSValue names = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const std::string &name: BlockPaletteRegistry::getInstance().getBlockNames())
            JS_SetPropertyUint32(ctx, names, index++, jsString(ctx, name));
        return names;
    }

    JSValue palettePermutations(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_UNDEFINED;

        const std::vector<Tag> *permutations =
                BlockPaletteRegistry::getInstance().getPermutations(toStdString(ctx, argv[0]));
        if (permutations == nullptr)
            return JS_UNDEFINED;

        JSValue list = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const Tag &states: *permutations) {
            JSValue object = JS_NewObject(ctx);
            const std::vector<std::string> &keys = states.getKeys();
            const std::vector<Tag> &values = states.getValues();
            for (size_t key = 0; key < keys.size(); ++key)
                JS_SetPropertyStr(ctx, object, keys[key].c_str(), stateValueToJs(ctx, values[key]));
            JS_SetPropertyUint32(ctx, list, index++, object);
        }
        return list;
    }

    JSValue itemTypeNames(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        JSValue names = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const std::shared_ptr<ItemDefinition> &definition: api->host().getItemDefinitions().getAll())
            JS_SetPropertyUint32(ctx, names, index++, jsString(ctx, definition->getIdentifier()));
        return names;
    }

    const char *DATA_BOOTSTRAP = R"JS(
(() => {
    const normalizeId = (id) => {
        const text = String(id);
        return text.includes(":") ? text : "minecraft:" + text;
    };

    const permutationCache = new Map();
    const permutationsOf = (id) => {
        if (!permutationCache.has(id)) {
            const list = __falconPalettePermutations(id);
            permutationCache.set(id, list === undefined ? null : list);
        }
        return permutationCache.get(id);
    };

    class BlockType {
        constructor(id) {
            this.id = id;
        }
    }

    class BlockPermutation {
        constructor(id, states) {
            this.type = new BlockType(id);
            this._states = states;
        }

        getAllStates() {
            return Object.assign({}, this._states);
        }

        getState(name) {
            return this._states[name];
        }

        withState(name, value) {
            return BlockPermutation.resolve(this.type.id, Object.assign({}, this._states, { [name]: value }));
        }

        matches(id, states) {
            if (normalizeId(id) !== this.type.id) {
                return false;
            }
            if (states) {
                for (const key of Object.keys(states)) {
                    if (this._states[key] !== states[key]) {
                        return false;
                    }
                }
            }
            return true;
        }

        static resolve(id, states) {
            const blockId = normalizeId(id);
            const list = permutationsOf(blockId);
            if (!list || list.length === 0) {
                throw new Error("Unknown block type " + blockId);
            }

            const wanted = Object.assign({}, list[0]);
            if (states) {
                for (const key of Object.keys(states)) {
                    if (!(key in wanted)) {
                        throw new Error("Block " + blockId + " has no state " + key);
                    }
                    wanted[key] = states[key];
                }
            }

            for (const candidate of list) {
                let same = true;
                for (const key of Object.keys(wanted)) {
                    if (candidate[key] !== wanted[key]) {
                        same = false;
                        break;
                    }
                }
                if (same) {
                    return new BlockPermutation(blockId, candidate);
                }
            }
            throw new Error("Invalid state value for block " + blockId);
        }
    }

    let blockTypes = null;
    const allBlockTypes = () => {
        if (blockTypes === null) {
            blockTypes = new Map();
            for (const name of __falconPaletteBlockNames()) {
                blockTypes.set(name, new BlockType(name));
            }
        }
        return blockTypes;
    };

    const BlockTypes = {
        get(id) {
            return allBlockTypes().get(normalizeId(id));
        },
        getAll() {
            return Array.from(allBlockTypes().values());
        }
    };

    const compareStateValues = (left, right) => {
        if (typeof left === typeof right && typeof left !== "string") {
            return Number(left) - Number(right);
        }
        return 0;
    };

    let blockStates = null;
    const allBlockStates = () => {
        if (blockStates === null) {
            const values = new Map();
            for (const name of __falconPaletteBlockNames()) {
                for (const states of permutationsOf(name) || []) {
                    for (const key of Object.keys(states)) {
                        if (!values.has(key)) {
                            values.set(key, []);
                        }
                        const known = values.get(key);
                        if (!known.includes(states[key])) {
                            known.push(states[key]);
                        }
                    }
                }
            }
            blockStates = new Map();
            for (const [key, known] of values) {
                blockStates.set(key, { id: key, validValues: known.slice().sort(compareStateValues) });
            }
        }
        return blockStates;
    };

    const BlockStates = {
        get(name) {
            return allBlockStates().get(String(name));
        },
        getAll() {
            return Array.from(allBlockStates().values());
        }
    };

    class ItemType {
        constructor(id) {
            this.id = id;
        }
    }

    let itemTypes = null;
    const allItemTypes = () => {
        if (itemTypes === null) {
            itemTypes = new Map();
            for (const name of __falconItemTypeNames()) {
                itemTypes.set(name, new ItemType(name));
            }
        }
        return itemTypes;
    };

    const ItemTypes = {
        get(id) {
            return allItemTypes().get(normalizeId(id));
        },
        getAll() {
            return Array.from(allItemTypes().values());
        }
    };

    globalThis.__falconServerData = { BlockPermutation, BlockStates, BlockTypes, ItemTypes };

    globalThis.__falconInstallJobs = (system) => {
        const JOB_BUDGET_MS = 5;
        const jobs = new Map();
        let nextJobId = 1;

        system.runJob = (generator) => {
            const jobId = nextJobId++;
            const finish = () => {
                const runId = jobs.get(jobId);
                if (runId !== undefined) {
                    system.clearRun(runId);
                    jobs.delete(jobId);
                }
            };
            const runId = system.runInterval(() => {
                const deadline = Date.now() + JOB_BUDGET_MS;
                try {
                    do {
                        if (generator.next().done) {
                            finish();
                            return;
                        }
                    } while (Date.now() < deadline);
                } catch (error) {
                    finish();
                    throw error;
                }
            }, 1);
            jobs.set(jobId, runId);
            return jobId;
        };

        system.clearJob = (jobId) => {
            const runId = jobs.get(jobId);
            if (runId !== undefined) {
                system.clearRun(runId);
                jobs.delete(jobId);
            }
        };
    };
})();
)JS";

    JSValue blockVolumeConstructor(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        JSValue object = JS_NewObject(ctx);
        if (argc >= 1)
            JS_SetPropertyStr(ctx, object, "from", JS_DupValue(ctx, argv[0]));
        if (argc >= 2)
            JS_SetPropertyStr(ctx, object, "to", JS_DupValue(ctx, argv[1]));
        return object;
    }

    JSValue entityWaypointConstructor(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        JSValue object = JS_NewObject(ctx);
        if (argc >= 1)
            JS_SetPropertyStr(ctx, object, "entity", JS_DupValue(ctx, argv[0]));
        if (argc >= 2)
            JS_SetPropertyStr(ctx, object, "textureSelector", JS_DupValue(ctx, argv[1]));
        if (argc >= 3)
            JS_SetPropertyStr(ctx, object, "entityRules", JS_DupValue(ctx, argv[2]));
        return object;
    }

    int moduleInit(JSContext *ctx, JSModuleDef *module) {
        ScriptApi *api = ScriptApi::fromRuntime(JS_GetRuntime(ctx));
        if (api == nullptr)
            return -1;

        JS_SetModuleExport(ctx, module, "world", JS_DupValue(ctx, api->moduleWorld()));
        JS_SetModuleExport(ctx, module, "system", JS_DupValue(ctx, api->moduleSystem()));

        JSValue itemStackConstructorValue =
                JS_NewCFunction2(ctx, itemStackConstructor, "ItemStack", 2, JS_CFUNC_constructor, 0);
        JS_SetModuleExport(ctx, module, "ItemStack", itemStackConstructorValue);

        JSValue global = JS_GetGlobalObject(ctx);
        JSValue serverData = JS_GetPropertyStr(ctx, global, "__falconServerData");
        JS_SetModuleExport(ctx, module, "BlockPermutation", JS_GetPropertyStr(ctx, serverData, "BlockPermutation"));
        JS_SetModuleExport(ctx, module, "BlockStates", JS_GetPropertyStr(ctx, serverData, "BlockStates"));
        JS_SetModuleExport(ctx, module, "BlockTypes", JS_GetPropertyStr(ctx, serverData, "BlockTypes"));
        JS_SetModuleExport(ctx, module, "ItemTypes", JS_GetPropertyStr(ctx, serverData, "ItemTypes"));
        JS_FreeValue(ctx, serverData);
        JS_FreeValue(ctx, global);

        JSValue equipmentSlot = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, equipmentSlot, "Head", JS_NewString(ctx, "Head"));
        JS_SetPropertyStr(ctx, equipmentSlot, "Chest", JS_NewString(ctx, "Chest"));
        JS_SetPropertyStr(ctx, equipmentSlot, "Legs", JS_NewString(ctx, "Legs"));
        JS_SetPropertyStr(ctx, equipmentSlot, "Feet", JS_NewString(ctx, "Feet"));
        JS_SetPropertyStr(ctx, equipmentSlot, "Mainhand", JS_NewString(ctx, "Mainhand"));
        JS_SetPropertyStr(ctx, equipmentSlot, "Offhand", JS_NewString(ctx, "Offhand"));
        JS_SetModuleExport(ctx, module, "EquipmentSlot", equipmentSlot);

        JSValue gameMode = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, gameMode, "survival", JS_NewString(ctx, "survival"));
        JS_SetPropertyStr(ctx, gameMode, "creative", JS_NewString(ctx, "creative"));
        JS_SetPropertyStr(ctx, gameMode, "adventure", JS_NewString(ctx, "adventure"));
        JS_SetPropertyStr(ctx, gameMode, "spectator", JS_NewString(ctx, "spectator"));
        JS_SetModuleExport(ctx, module, "GameMode", gameMode);

        JSValue playerClass = JS_NewCFunction(ctx, noop, "Player", 0);
        JS_SetPropertyStr(ctx, playerClass, "prototype", JS_DupValue(ctx, api->playerPrototype()));
        JS_SetModuleExport(ctx, module, "Player", playerClass);

        JSValue inputCategory = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, inputCategory, "Camera", JS_NewInt32(ctx, 1));
        JS_SetPropertyStr(ctx, inputCategory, "Movement", JS_NewInt32(ctx, 2));
        JS_SetPropertyStr(ctx, inputCategory, "Jump", JS_NewInt32(ctx, 8));
        JS_SetPropertyStr(ctx, inputCategory, "Sneak", JS_NewInt32(ctx, 9));
        JS_SetPropertyStr(ctx, inputCategory, "Mount", JS_NewInt32(ctx, 10));
        JS_SetPropertyStr(ctx, inputCategory, "Dismount", JS_NewInt32(ctx, 11));
        JS_SetModuleExport(ctx, module, "InputPermissionCategory", inputCategory);

        JSValue direction = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, direction, "Down", JS_NewString(ctx, "Down"));
        JS_SetPropertyStr(ctx, direction, "Up", JS_NewString(ctx, "Up"));
        JS_SetPropertyStr(ctx, direction, "North", JS_NewString(ctx, "North"));
        JS_SetPropertyStr(ctx, direction, "South", JS_NewString(ctx, "South"));
        JS_SetPropertyStr(ctx, direction, "East", JS_NewString(ctx, "East"));
        JS_SetPropertyStr(ctx, direction, "West", JS_NewString(ctx, "West"));
        JS_SetModuleExport(ctx, module, "Direction", direction);

        JS_SetModuleExport(ctx, module, "BlockVolume",
                           JS_NewCFunction2(ctx, blockVolumeConstructor, "BlockVolume", 2, JS_CFUNC_constructor, 0));
        JS_SetModuleExport(ctx, module, "EntityWaypoint",
                           JS_NewCFunction2(ctx, entityWaypointConstructor, "EntityWaypoint", 3,
                                            JS_CFUNC_constructor, 0));

        JSValue inventoryType = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, inventoryType, "Inventory", JS_NewString(ctx, "inventory"));
        JS_SetModuleExport(ctx, module, "PlayerInventoryType", inventoryType);

        JS_SetModuleExport(ctx, module, "TickingAreaManager", JS_NewObject(ctx));
        return 0;
    }
}

void ScriptApi::_installDataBootstrap() {
    JSValue global = JS_GetGlobalObject(mContext);
    JS_SetPropertyStr(mContext, global, "__falconPaletteBlockNames",
                      JS_NewCFunction(mContext, paletteBlockNames, "__falconPaletteBlockNames", 0));
    JS_SetPropertyStr(mContext, global, "__falconPalettePermutations",
                      JS_NewCFunction(mContext, palettePermutations, "__falconPalettePermutations", 1));
    JS_SetPropertyStr(mContext, global, "__falconItemTypeNames",
                      JS_NewCFunction(mContext, itemTypeNames, "__falconItemTypeNames", 0));

    JSValue result = JS_Eval(mContext, DATA_BOOTSTRAP, strlen(DATA_BOOTSTRAP), "<falcon-data>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(mContext);
        const char *message = JS_ToCString(mContext, exception);
        LOG_ERROR(LogAreaID::Server, "Data bootstrap error: %s", message == nullptr ? "?" : message);
        if (message != nullptr)
            JS_FreeCString(mContext, message);
        JS_FreeValue(mContext, exception);
    }
    JS_FreeValue(mContext, result);

    JSValue installJobs = JS_GetPropertyStr(mContext, global, "__falconInstallJobs");
    if (JS_IsFunction(mContext, installJobs)) {
        JSValue systemValue = JS_DupValue(mContext, mSystem);
        JSValue installed = JS_Call(mContext, installJobs, JS_UNDEFINED, 1, &systemValue);
        JS_FreeValue(mContext, installed);
        JS_FreeValue(mContext, systemValue);
    }
    JS_FreeValue(mContext, installJobs);
    JS_FreeValue(mContext, global);
}

void ScriptApi::_registerModule() {
    _installDataBootstrap();

    JSModuleDef *module = JS_NewCModule(mContext, "@minecraft/server", moduleInit);
    if (module == nullptr)
        return;

    JS_AddModuleExport(mContext, module, "world");
    JS_AddModuleExport(mContext, module, "system");
    JS_AddModuleExport(mContext, module, "ItemStack");
    JS_AddModuleExport(mContext, module, "BlockPermutation");
    JS_AddModuleExport(mContext, module, "BlockStates");
    JS_AddModuleExport(mContext, module, "BlockTypes");
    JS_AddModuleExport(mContext, module, "ItemTypes");
    JS_AddModuleExport(mContext, module, "EquipmentSlot");
    JS_AddModuleExport(mContext, module, "GameMode");
    JS_AddModuleExport(mContext, module, "Player");
    JS_AddModuleExport(mContext, module, "InputPermissionCategory");
    JS_AddModuleExport(mContext, module, "Direction");
    JS_AddModuleExport(mContext, module, "BlockVolume");
    JS_AddModuleExport(mContext, module, "EntityWaypoint");
    JS_AddModuleExport(mContext, module, "PlayerInventoryType");
    JS_AddModuleExport(mContext, module, "TickingAreaManager");
}

void ScriptApi::install() {
    JS_SetRuntimeOpaque(mRuntime, this);

    JS_NewClassID(mRuntime, &mPlayerClassId);
    JSClassDef playerClass{};
    playerClass.class_name = "Player";
    JS_NewClass(mRuntime, mPlayerClassId, &playerClass);

    mPlayerPrototype = JS_NewObject(mContext);
    JS_SetPropertyStr(mContext, mPlayerPrototype, "sendMessage",
                      JS_NewCFunction(mContext, playerSendMessage, "sendMessage", 1));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "getGameMode",
                      JS_NewCFunction(mContext, playerGetGameMode, "getGameMode", 0));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "kick",
                      JS_NewCFunction(mContext, playerKick, "kick", 1));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "teleport",
                      JS_NewCFunction(mContext, playerTeleport, "teleport", 1));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "runCommand",
                      JS_NewCFunction(mContext, playerRunCommand, "runCommand", 1));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "getComponent",
                      JS_NewCFunction(mContext, playerGetComponent, "getComponent", 1));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "hasComponent",
                      JS_NewCFunction(mContext, playerHasComponent, "hasComponent", 1));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "addTag",
                      JS_NewCFunction(mContext, playerAddTag, "addTag", 1));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "removeTag",
                      JS_NewCFunction(mContext, playerRemoveTag, "removeTag", 1));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "hasTag",
                      JS_NewCFunction(mContext, playerHasTag, "hasTag", 1));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "getTags",
                      JS_NewCFunction(mContext, playerGetTags, "getTags", 0));

    JSAtom typeIdAtom = JS_NewAtom(mContext, "typeId");
    JS_DefinePropertyGetSet(mContext, mPlayerPrototype, typeIdAtom,
                            JS_NewCFunction(mContext, playerGetTypeId, "get typeId", 0), JS_UNDEFINED,
                            JS_PROP_CONFIGURABLE);
    JS_FreeAtom(mContext, typeIdAtom);

    JSAtom playerIdAtom = JS_NewAtom(mContext, "id");
    JS_DefinePropertyGetSet(mContext, mPlayerPrototype, playerIdAtom,
                            JS_NewCFunction(mContext, playerGetId, "get id", 0), JS_UNDEFINED,
                            JS_PROP_CONFIGURABLE);
    JS_FreeAtom(mContext, playerIdAtom);
    JS_SetPropertyStr(mContext, mPlayerPrototype, "getDynamicProperty",
                      JS_NewCFunction(mContext, playerGetDynamicProperty, "getDynamicProperty", 1));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "setDynamicProperty",
                      JS_NewCFunction(mContext, playerSetDynamicProperty, "setDynamicProperty", 2));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "getDynamicPropertyIds",
                      JS_NewCFunction(mContext, playerGetDynamicPropertyIds, "getDynamicPropertyIds", 0));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "clearDynamicProperties",
                      JS_NewCFunction(mContext, playerClearDynamicProperties, "clearDynamicProperties", 0));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "getViewDirection",
                      JS_NewCFunction(mContext, playerGetViewDirection, "getViewDirection", 0));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "getHeadLocation",
                      JS_NewCFunction(mContext, playerGetHeadLocation, "getHeadLocation", 0));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "getRotation",
                      JS_NewCFunction(mContext, playerGetRotationMethod, "getRotation", 0));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "playAnimation",
                      JS_NewCFunction(mContext, playerPlayAnimationMethod, "playAnimation", 1));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "getItemCooldown",
                      JS_NewCFunction(mContext, playerGetItemCooldown, "getItemCooldown", 1));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "setGameMode",
                      JS_NewCFunction(mContext, playerSetGameMode, "setGameMode", 1));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "playSound",
                      JS_NewCFunction(mContext, playerPlaySound, "playSound", 2));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "spawnParticle",
                      JS_NewCFunction(mContext, playerSpawnParticle, "spawnParticle", 2));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "startItemCooldown",
                      JS_NewCFunction(mContext, playerStartItemCooldown, "startItemCooldown", 2));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "addExperience",
                      JS_NewCFunction(mContext, playerAddExperience, "addExperience", 1));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "addLevels",
                      JS_NewCFunction(mContext, playerAddLevels, "addLevels", 1));
    JS_SetPropertyStr(mContext, mPlayerPrototype, "resetLevel",
                      JS_NewCFunction(mContext, playerResetLevel, "resetLevel", 0));

    static const char *playerGetterNames[] = {"isSneaking", "isSprinting", "selectedSlotIndex",
                                              "onScreenDisplay", "inputPermissions", "locatorBar",
                                              "level", "camera"};
    JSCFunction *playerGetters[] = {playerGetIsSneaking, playerGetIsSprinting, playerGetSelectedSlot,
                                    playerGetOnScreenDisplay, playerGetInputPermissions, playerGetLocatorBar,
                                    playerGetLevel, playerGetCamera};
    for (int i = 0; i < 8; ++i) {
        JSAtom atom = JS_NewAtom(mContext, playerGetterNames[i]);
        JS_DefinePropertyGetSet(mContext, mPlayerPrototype, atom,
                                JS_NewCFunction(mContext, playerGetters[i], "get", 0), JS_UNDEFINED,
                                JS_PROP_CONFIGURABLE);
        JS_FreeAtom(mContext, atom);
    }

    JSAtom locationAtom = JS_NewAtom(mContext, "location");
    JS_DefinePropertyGetSet(mContext, mPlayerPrototype, locationAtom,
                            JS_NewCFunction(mContext, playerGetLocation, "get location", 0), JS_UNDEFINED,
                            JS_PROP_CONFIGURABLE);
    JS_FreeAtom(mContext, locationAtom);

    JSAtom dimensionAtom = JS_NewAtom(mContext, "dimension");
    JS_DefinePropertyGetSet(mContext, mPlayerPrototype, dimensionAtom,
                            JS_NewCFunction(mContext, playerGetDimension, "get dimension", 0), JS_UNDEFINED,
                            JS_PROP_CONFIGURABLE);
    JS_FreeAtom(mContext, dimensionAtom);

    JSAtom nameAtom = JS_NewAtom(mContext, "name");
    JSValue nameGetter = JS_NewCFunction(mContext, playerGetNameMethod, "get name", 0);
    JS_DefinePropertyGetSet(mContext, mPlayerPrototype, nameAtom, nameGetter, JS_UNDEFINED,
                            JS_PROP_CONFIGURABLE);
    JS_FreeAtom(mContext, nameAtom);

    JS_SetClassProto(mContext, mPlayerClassId, JS_DupValue(mContext, mPlayerPrototype));

    _buildActorClass();
    _buildBlockClass();
    _buildItemStackClass();
    _buildEvents();
    _buildSystem();
    _buildWorld();
    _registerModule();
    _installUiBootstrap();
    _registerUiModule();

    JSValue global = JS_GetGlobalObject(mContext);
    JSValue makeScoreboard = JS_GetPropertyStr(mContext, global, "__falconMakeScoreboard");
    if (JS_IsFunction(mContext, makeScoreboard)) {
        JSValue scoreboard = JS_Call(mContext, makeScoreboard, JS_UNDEFINED, 0, nullptr);
        JS_SetPropertyStr(mContext, mWorld, "scoreboard", scoreboard);
    }
    JS_FreeValue(mContext, makeScoreboard);

    JSValue makeTicking = JS_GetPropertyStr(mContext, global, "__falconMakeTickingAreaManager");
    if (JS_IsFunction(mContext, makeTicking)) {
        JSValue tickingManager = JS_Call(mContext, makeTicking, JS_UNDEFINED, 0, nullptr);
        JS_SetPropertyStr(mContext, mWorld, "tickingAreaManager", tickingManager);
    }
    JS_FreeValue(mContext, makeTicking);
    JS_FreeValue(mContext, global);

    _subscribeGameEvents();
}

void ScriptApi::tick(int64_t currentTick) {
    mScheduler.tick(currentTick);
}
