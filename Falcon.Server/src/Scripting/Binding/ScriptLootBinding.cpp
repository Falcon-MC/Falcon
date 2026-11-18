#include "Scripting/Binding/ScriptApi.h"

#include "Core/Json/Json.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Actor.h"
#include "World/Actor/ServerActor.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Level/Block/BlockData.h"
#include "World/Level/Level.h"
#include "World/Level/Storage/Loot/LootItems.h"
#include "World/Level/Storage/Loot/LootTableRegistry.h"

#include <random>

using namespace ScriptValues;

namespace {
    JSValue lootHas(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        return JS_NewBool(ctx, argc >= 1 && ScriptApi::fromContext(ctx)->surface().has(toString(ctx, argv[0])));
    }

    JSValue typeError(JSContext *ctx) {
        return JS_ThrowTypeError(ctx, "Native type conversion failed.");
    }

    JSValue toJs(JSContext *ctx, const json::Value &value) {
        switch (value.mType) {
            case json::Value::Type::Boolean:
                return JS_NewBool(ctx, value.mBoolean);
            case json::Value::Type::Number:
                return JS_NewFloat64(ctx, value.mNumber);
            case json::Value::Type::String:
                return string(ctx, value.mString);
            case json::Value::Type::Array: {
                JSValue array = JS_NewArray(ctx);
                for (uint32_t index = 0; index < value.mArray.size(); ++index)
                    JS_SetPropertyUint32(ctx, array, index, toJs(ctx, *value.mArray[index]));
                return array;
            }
            case json::Value::Type::Object: {
                JSValue object = JS_NewObject(ctx);
                for (const std::string &key: value.mKeys) {
                    const json::Value *member = value.get(key);
                    if (member != nullptr)
                        JS_SetPropertyStr(ctx, object, key.c_str(), toJs(ctx, *member));
                }
                return object;
            }
            default:
                return JS_NULL;
        }
    }

    // an absent tool is the bare hand; false when the value is no ItemStack
    bool readTool(JSContext *ctx, JSValueConst value, ItemStack &tool) {
        tool = ItemStack::air();
        if (JS_IsUndefined(value))
            return true;
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptItemRef *ref = api->native<ScriptItemRef>(value);
        return ref != nullptr && ref->mComponentId.empty() && api->toItemStack(*ref->mItem, tool);
    }

    JSValue itemArray(JSContext *ctx, const std::vector<ItemStack> &items) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const ItemStack &item: items) {
            if (!item.isAir() && item.mCount > 0)
                JS_SetPropertyUint32(ctx, array, index++, api->makeItem(item));
        }
        return array;
    }

    JSValue rollTable(JSContext *ctx, const LootTable *table, const ItemStack &tool) {
        std::vector<ItemStack> items;
        if (table != nullptr) {
            std::mt19937 random(std::random_device{}());
            LootContext context(random);
            context.mLootingLevel = ItemEnchantments::getLevel(tool, EnchantmentIds::LOOTING);
            ServerNetworkHandler &host = ScriptApi::fromContext(ctx)->host();
            for (const LootDrop &drop: table->roll(context))
                items.push_back(LootItems::toItemStack(host, drop));
        }
        return itemArray(ctx, items);
    }

    // undefined when the tool cannot harvest the block, as the game answers
    JSValue blockLoot(JSContext *ctx, Level *level, const Vector3i &position, const BlockState &state,
                      const ItemStack &tool) {
        const BlockData *data = BlockDataTable::find(state.mName.c_str());
        if (data != nullptr && !BlockActionHandler::canHarvest(*data, tool))
            return JS_UNDEFINED;
        ServerNetworkHandler &host = ScriptApi::fromContext(ctx)->host();
        return itemArray(ctx, BlockActionHandler::blockDrops(host, level, position, state, tool));
    }

    JSValue lootDefinition(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        if (argc < 1 || !JS_IsString(argv[0]))
            return typeError(ctx);
        const json::Value *definition = LootTableRegistry::getInstance().getDefinition(toString(ctx, argv[0]) + ".json");
        return definition == nullptr || !definition->isObject() ? JS_UNDEFINED : toJs(ctx, *definition);
    }

    JSValue lootIsItemStack(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        const ScriptItemRef *ref = argc >= 1 ? ScriptApi::fromContext(ctx)->native<ScriptItemRef>(argv[0]) : nullptr;
        return JS_NewBool(ctx, ref != nullptr && ref->mComponentId.empty());
    }

    JSValue lootFromTable(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ItemStack tool;
        if (argc < 1 || !JS_IsString(argv[0]) || !readTool(ctx, argc >= 2 ? argv[1] : JS_UNDEFINED, tool))
            return typeError(ctx);
        return rollTable(ctx, LootTableRegistry::getInstance().get(toString(ctx, argv[0]) + ".json"), tool);
    }

    JSValue lootFromBlock(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptBlockRef *ref = argc >= 1 ? api->native<ScriptBlockRef>(argv[0]) : nullptr;
        ItemStack tool;
        if (ref == nullptr || !readTool(ctx, argc >= 2 ? argv[1] : JS_UNDEFINED, tool))
            return typeError(ctx);
        Level &level = api->resolveLevel(ref->mWorldId, ref->mDimension);
        if (ref->mY < level.getMinY() || ref->mY > level.getMaxY())
            return api->throwError("LocationOutOfWorldBoundariesError", "");
        if (level.peekChunkPtr(ref->mX >> 4, ref->mZ >> 4) == nullptr)
            return api->throwError("LocationInUnloadedChunkError", "");
        const Vector3i position(ref->mX, ref->mY, ref->mZ);
        return blockLoot(ctx, &level, position, level.getBlockState(position.x, position.y, position.z), tool);
    }

    JSValue lootFromPermutation(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        BlockState state;
        ItemStack tool;
        if (argc < 1 || !api->readPermutation(argv[0], state) || !readTool(ctx, argc >= 2 ? argv[1] : JS_UNDEFINED, tool))
            return typeError(ctx);
        return blockLoot(ctx, nullptr, Vector3i(0, 0, 0), state, tool);
    }

    JSValue lootFromBlockType(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ItemStack tool;
        if (argc < 1 || !JS_IsString(argv[0]) || !readTool(ctx, argc >= 2 ? argv[1] : JS_UNDEFINED, tool))
            return typeError(ctx);
        return blockLoot(ctx, nullptr, Vector3i(0, 0, 0), ScriptApi::defaultBlockState(toString(ctx, argv[0])), tool);
    }

    JSValue lootFromEntity(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptEntityRef *ref = argc >= 1 ? api->native<ScriptEntityRef>(argv[0]) : nullptr;
        ItemStack tool;
        if (ref == nullptr || !readTool(ctx, argc >= 2 ? argv[1] : JS_UNDEFINED, tool))
            return typeError(ctx);
        Actor *actor = api->resolveEntity(*ref);
        if (actor == nullptr)
            return api->throwInvalidEntity("call", "generateLootFromEntity", *ref);
        const ServerActor *server = dynamic_cast<const ServerActor *>(actor);
        const std::string type = server != nullptr ? server->getTypeId() : std::string("minecraft:player");
        return rollTable(ctx, LootTableRegistry::getInstance().getForEntity(type), tool);
    }

    JSValue lootFromEntityType(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptNamedRef *ref = argc >= 1 ? api->native<ScriptNamedRef>(argv[0]) : nullptr;
        ItemStack tool;
        if (ref == nullptr || !readTool(ctx, argc >= 2 ? argv[1] : JS_UNDEFINED, tool))
            return typeError(ctx);
        return rollTable(ctx, LootTableRegistry::getInstance().getForEntity(ref->mName), tool);
    }

    JSValue worldGetLootTableManager(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return JS_DupValue(ctx, ScriptApi::fromContext(ctx)->lootTableManager());
    }

    void addNative(JSContext *ctx, JSValue object, const char *name, JSCFunction *function, int length) {
        JS_SetPropertyStr(ctx, object, name, JS_NewCFunction(ctx, function, name, length));
    }
}

JSValue ScriptApi::_lootNatives(JSValue itemTypes) {
    JSValue natives = JS_NewObject(mContext);
    JS_SetPropertyStr(mContext, natives, "ItemTypes", itemTypes);
    addNative(mContext, natives, "definition", lootDefinition, 1);
    addNative(mContext, natives, "has", lootHas, 1);
    addNative(mContext, natives, "isItemStack", lootIsItemStack, 1);
    addNative(mContext, natives, "fromTable", lootFromTable, 2);
    addNative(mContext, natives, "fromBlock", lootFromBlock, 2);
    addNative(mContext, natives, "fromPermutation", lootFromPermutation, 2);
    addNative(mContext, natives, "fromBlockType", lootFromBlockType, 2);
    addNative(mContext, natives, "fromEntity", lootFromEntity, 2);
    addNative(mContext, natives, "fromEntityType", lootFromEntityType, 2);
    return natives;
}

void ScriptApi::_bindLootTableManager() {
    method(prototypeOf("World"), "World", "getLootTableManager", worldGetLootTableManager, 0);
}
