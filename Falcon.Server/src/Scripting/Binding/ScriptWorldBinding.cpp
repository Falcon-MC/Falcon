#include "Scripting/Binding/ScriptApi.h"

#include "World/Actor/ServerActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/CraftingRecipeTable.h"
#include "World/Level/Level.h"
#include "World/Level/WorldManager.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>
#include <cstdlib>

using namespace ScriptValues;

namespace {
    JSValue systemRun(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1 || !JS_IsFunction(ctx, argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

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
        ScriptApi *api = ScriptApi::fromContext(ctx);
        uint32_t id = 0;
        if (argc >= 1 && JS_ToUint32(ctx, &id, argv[0]) == 0)
            api->scheduler().clear(id);
        return JS_UNDEFINED;
    }

    JSValue systemGetCurrentTick(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return JS_NewInt64(ctx, ScriptApi::fromContext(ctx)->host().getCurrentTick());
    }

    JSValue holderGetter(JSContext *ctx, JSValueConst, int, JSValueConst *, int magic) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        switch (magic) {
            case 0:
                return JS_DupValue(ctx, api->worldAfterEvents());
            case 1:
                return JS_DupValue(ctx, api->worldBeforeEvents());
            case 2:
                return JS_DupValue(ctx, api->systemAfterEvents());
            case 3:
                return JS_DupValue(ctx, api->systemBeforeEvents());
            case 4:
                return JS_DupValue(ctx, api->moduleScoreboard());
            default:
                return JS_DupValue(ctx, api->moduleTickingAreas());
        }
    }

    JSValue worldSendMessage(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
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
        ScriptApi *api = ScriptApi::fromContext(ctx);
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (auto &entry: api->host().getPlayers()) {
            if (entry.second.isSpawned())
                JS_SetPropertyUint32(ctx, array, index++, api->makePlayer(entry.second));
        }
        return array;
    }

    JSValue worldGetDimension(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        std::string identifier = toString(ctx, argv[0]);
        if (identifier.find(':') == std::string::npos)
            identifier = "minecraft:" + identifier;

        if (identifier == "minecraft:nether")
            return api->makeDimension(api->host().getDimension(DimensionType::Nether));
        if (identifier == "minecraft:the_end")
            return api->makeDimension(api->host().getDimension(DimensionType::TheEnd));
        if (identifier == "minecraft:overworld")
            return api->makeDimension(api->host().getDimension(DimensionType::Overworld));
        return JS_ThrowTypeError(ctx, "Unknown dimension %s", identifier.c_str());
    }

    JSValue worldGetDynamicProperty(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1)
            return JS_UNDEFINED;
        return dynamicPropertyGet(ctx, api->host().getWorldDynamicProperties(), toString(ctx, argv[0]));
    }

    JSValue worldSetDynamicProperty(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc >= 1)
            dynamicPropertySet(ctx, api->host().getWorldDynamicProperties(), toString(ctx, argv[0]),
                               argc >= 2 ? argv[1] : JS_UNDEFINED);
        return JS_UNDEFINED;
    }

    JSValue worldGetDynamicPropertyIds(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return dynamicPropertyIds(ctx, ScriptApi::fromContext(ctx)->host().getWorldDynamicProperties());
    }

    JSValue worldClearDynamicProperties(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        ScriptApi::fromContext(ctx)->host().getWorldDynamicProperties().clear();
        return JS_UNDEFINED;
    }

    JSValue worldGetEntity(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1)
            return JS_UNDEFINED;

        const std::string idText = toString(ctx, argv[0]);
        char *end = nullptr;
        const int64_t uniqueId = (int64_t) strtoll(idText.c_str(), &end, 10);
        if (end == idText.c_str())
            return JS_UNDEFINED;

        for (auto &entry: api->host().getPlayers()) {
            if (entry.second.isSpawned() && entry.second.getUniqueId() == uniqueId)
                return api->makePlayer(entry.second);
        }
        ServerActor *actor = api->host().getActor(uniqueId);
        if (actor == nullptr || !actor->isAlive())
            return JS_UNDEFINED;
        return api->makeActor(*actor);
    }

    ScriptBlockRef *blockOf(JSContext *ctx, JSValueConst thisVal) {
        return ScriptApi::fromContext(ctx)->native<ScriptBlockRef>(thisVal);
    }

    Level &blockLevel(JSContext *ctx, const ScriptBlockRef &block) {
        return ScriptApi::fromContext(ctx)->resolveLevel(block.mWorldId, block.mDimension);
    }

    std::string blockIdentifier(JSContext *ctx, const ScriptBlockRef &block) {
        return blockLevel(ctx, block).getBlockState(block.mX, block.mY, block.mZ).mName;
    }

    JSValue blockGetCoord(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        const ScriptBlockRef *block = blockOf(ctx, thisVal);
        return JS_NewInt32(ctx, magic == 0 ? block->mX : (magic == 1 ? block->mY : block->mZ));
    }

    JSValue blockGetTypeId(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return string(ctx, blockIdentifier(ctx, *blockOf(ctx, thisVal)));
    }

    JSValue blockGetIsAir(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewBool(ctx, blockIdentifier(ctx, *blockOf(ctx, thisVal)) == "minecraft:air");
    }

    JSValue blockGetPermutation(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return ScriptApi::fromContext(ctx)->makePermutation(blockIdentifier(ctx, *blockOf(ctx, thisVal)));
    }

    JSValue blockGetIsValid(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptBlockRef *block = blockOf(ctx, thisVal);
        return JS_NewBool(ctx, blockLevel(ctx, *block).peekBlockPtr(block->mX, block->mY, block->mZ) != nullptr);
    }

    JSValue blockMatches(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_FALSE;
        std::string target = toString(ctx, argv[0]);
        if (target.find(':') == std::string::npos)
            target = "minecraft:" + target;
        return JS_NewBool(ctx, blockIdentifier(ctx, *blockOf(ctx, thisVal)) == target);
    }

    JSValue blockGetLocation(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptBlockRef *block = blockOf(ctx, thisVal);
        return vector3(ctx, (float) block->mX, (float) block->mY, (float) block->mZ);
    }

    JSValue blockGetDimension(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return ScriptApi::fromContext(ctx)->makeDimension(blockLevel(ctx, *blockOf(ctx, thisVal)));
    }

    JSValue blockCenter(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptBlockRef *block = blockOf(ctx, thisVal);
        return vector3(ctx, (float) block->mX + 0.5f, (float) block->mY + 0.5f, (float) block->mZ + 0.5f);
    }

    JSValue blockOffset(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic) {
        const ScriptBlockRef *block = blockOf(ctx, thisVal);
        int32_t steps = 1;
        if (argc >= 1 && !JS_IsUndefined(argv[0]))
            JS_ToInt32(ctx, &steps, argv[0]);

        int32_t x = block->mX, y = block->mY, z = block->mZ;
        switch (magic) {
            case 0: y += steps; break;
            case 1: y -= steps; break;
            case 2: z -= steps; break;
            case 3: z += steps; break;
            case 4: x += steps; break;
            case 5: x -= steps; break;
            default: break;
        }
        return ScriptApi::fromContext(ctx)->makeBlock(blockLevel(ctx, *block), x, y, z);
    }

    std::string typeIdentifier(JSContext *ctx, JSValueConst value) {
        if (!JS_IsObject(value))
            return toString(ctx, value);

        JSValue typeValue = JS_GetPropertyStr(ctx, value, "type");
        JSValue idValue = JS_GetPropertyStr(ctx, JS_IsObject(typeValue) ? typeValue : value, "id");
        const std::string identifier = toString(ctx, idValue);
        JS_FreeValue(ctx, idValue);
        JS_FreeValue(ctx, typeValue);
        return identifier;
    }

    JSValue blockSetType(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const ScriptBlockRef *block = blockOf(ctx, thisVal);
        if (argc < 1)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        ScriptApi::fromContext(ctx)->setBlockType(blockLevel(ctx, *block), block->mX, block->mY, block->mZ,
                                                  typeIdentifier(ctx, argv[0]));
        return JS_UNDEFINED;
    }

    JSValue blockSetPermutation(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const ScriptBlockRef *block = blockOf(ctx, thisVal);
        if (argc < 1 || !JS_IsObject(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        // TODO: apply the permutation's states, the type only for now
        const std::string identifier = typeIdentifier(ctx, argv[0]);
        if (!identifier.empty())
            ScriptApi::fromContext(ctx)->setBlockType(blockLevel(ctx, *block), block->mX, block->mY, block->mZ,
                                                      identifier);
        return JS_UNDEFINED;
    }

    // TODO: volume and waypoint behaviour, the constructor arguments only for now
    JSValue blockVolumeConstructor(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        JSValue object = ScriptApi::fromContext(ctx)->makeInstance("BlockVolume", new ScriptNative());
        if (argc >= 1)
            JS_SetPropertyStr(ctx, object, "from", JS_DupValue(ctx, argv[0]));
        if (argc >= 2)
            JS_SetPropertyStr(ctx, object, "to", JS_DupValue(ctx, argv[1]));
        return object;
    }

    JSValue entityWaypointConstructor(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        JSValue object = ScriptApi::fromContext(ctx)->makeInstance("EntityWaypoint", new ScriptNative());
        if (argc >= 1)
            JS_SetPropertyStr(ctx, object, "entity", JS_DupValue(ctx, argv[0]));
        if (argc >= 2)
            JS_SetPropertyStr(ctx, object, "textureSelector", JS_DupValue(ctx, argv[1]));
        if (argc >= 3)
            JS_SetPropertyStr(ctx, object, "entityRules", JS_DupValue(ctx, argv[2]));
        return object;
    }

    ScriptItemData *itemOf(JSContext *ctx, JSValueConst thisVal) {
        const ScriptItemRef *ref = ScriptApi::fromContext(ctx)->native<ScriptItemRef>(thisVal);
        return ref == nullptr ? nullptr : ref->mItem.get();
    }

    JSValue itemStackConstructor(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        std::string typeId = typeIdentifier(ctx, argv[0]);
        if (typeId.empty())
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (typeId.find(':') == std::string::npos)
            typeId = "minecraft:" + typeId;

        int32_t amount = 1;
        if (argc >= 2 && !JS_IsUndefined(argv[1]))
            JS_ToInt32(ctx, &amount, argv[1]);
        return api->makeItemStack(typeId, amount < 1 ? 1 : amount);
    }

    JSValue itemStackGetTypeId(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return string(ctx, itemOf(ctx, thisVal)->mTypeId);
    }

    JSValue itemStackGetAmount(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewInt32(ctx, itemOf(ctx, thisVal)->mAmount);
    }

    JSValue itemStackSetAmount(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        int32_t amount = 1;
        if (argc >= 1)
            JS_ToInt32(ctx, &amount, argv[0]);
        itemOf(ctx, thisVal)->mAmount = amount;
        return JS_UNDEFINED;
    }

    JSValue itemStackGetDynamicProperty(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_UNDEFINED;
        return dynamicPropertyGet(ctx, itemOf(ctx, thisVal)->mDynamicProperties, toString(ctx, argv[0]));
    }

    JSValue itemStackSetDynamicProperty(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        if (argc >= 1)
            dynamicPropertySet(ctx, itemOf(ctx, thisVal)->mDynamicProperties, toString(ctx, argv[0]),
                               argc >= 2 ? argv[1] : JS_UNDEFINED);
        return JS_UNDEFINED;
    }

    JSValue itemStackGetDynamicPropertyIds(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return dynamicPropertyIds(ctx, itemOf(ctx, thisVal)->mDynamicProperties);
    }

    JSValue itemStackGetTags(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        JSValue tags = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const std::string &tag: CraftingRecipeTable::getItemTags(itemOf(ctx, thisVal)->mTypeId))
            JS_SetPropertyUint32(ctx, tags, index++, string(ctx, tag));
        return tags;
    }

    JSValue itemStackHasTag(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_FALSE;
        const std::string wanted = toString(ctx, argv[0]);
        for (const std::string &tag: CraftingRecipeTable::getItemTags(itemOf(ctx, thisVal)->mTypeId)) {
            if (tag == wanted)
                return JS_TRUE;
        }
        return JS_FALSE;
    }

    // the item components a stack of this type carries; getComponent and hasComponent both answer from here
    JSValue makeItemComponent(JSContext *ctx, JSValueConst thisVal, const std::string &requested) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptItemRef *item = api->native<ScriptItemRef>(thisVal);
        const std::string id = requested.find(':') == std::string::npos ? "minecraft:" + requested : requested;
        const CustomItemDefinition *definition = CustomContentRegistry::getInstance().findItem(item->mItem->mTypeId);

        const char *className = nullptr;
        if (id == "minecraft:durability" && definition != nullptr && definition->mMaxDurability > 0)
            className = "ItemDurabilityComponent";
        else if (id == "minecraft:enchantable")
            className = "ItemEnchantableComponent";
        else if (id == "minecraft:cooldown" && definition != nullptr
                 && (definition->mCooldownTicks > 0 || !definition->mCooldownCategory.empty()))
            className = "ItemCooldownComponent";
        if (className == nullptr)
            return JS_UNDEFINED;

        ScriptItemRef *component = new ScriptItemRef();
        component->mItem = item->mItem;
        component->mComponentId = id;
        return api->makeInstance(className, component);
    }

    JSValue itemStackGetComponent(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_UNDEFINED;
        return makeItemComponent(ctx, thisVal, toString(ctx, argv[0]));
    }

    JSValue itemStackHasComponent(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_FALSE;
        JSValue component = makeItemComponent(ctx, thisVal, toString(ctx, argv[0]));
        const bool has = !JS_IsUndefined(component);
        JS_FreeValue(ctx, component);
        return JS_NewBool(ctx, has);
    }

    const CustomItemDefinition *componentDefinition(JSContext *ctx, JSValueConst thisVal) {
        return CustomContentRegistry::getInstance().findItem(itemOf(ctx, thisVal)->mTypeId);
    }

    JSValue durabilityGetDamage(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewInt32(ctx, itemOf(ctx, thisVal)->mDamage);
    }

    JSValue durabilitySetDamage(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        int32_t value = 0;
        if (argc >= 1)
            JS_ToInt32(ctx, &value, argv[0]);
        itemOf(ctx, thisVal)->mDamage = value < 0 ? 0 : value;
        return JS_UNDEFINED;
    }

    JSValue durabilityGetMax(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const CustomItemDefinition *definition = componentDefinition(ctx, thisVal);
        return JS_NewInt32(ctx, definition == nullptr ? 0 : definition->mMaxDurability);
    }

    // TODO: the game's unbreaking chance range, a plain 1 / (level + 1) for now
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

    JSValue cooldownGetTicks(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const CustomItemDefinition *definition = componentDefinition(ctx, thisVal);
        return JS_NewInt32(ctx, definition == nullptr ? 0 : definition->mCooldownTicks);
    }

    JSValue cooldownGetCategory(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const CustomItemDefinition *definition = componentDefinition(ctx, thisVal);
        return string(ctx, definition == nullptr ? std::string() : definition->mCooldownCategory);
    }

    JSValue dimensionGetId(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptDimensionRef *ref = ScriptApi::fromContext(ctx)->native<ScriptDimensionRef>(thisVal);
        if (ref->mDimension == DimensionType::Nether)
            return JS_NewString(ctx, "minecraft:nether");
        if (ref->mDimension == DimensionType::TheEnd)
            return JS_NewString(ctx, "minecraft:the_end");
        return JS_NewString(ctx, "minecraft:overworld");
    }

    JSValue dimensionGetBlock(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        double x = 0.0, y = 0.0, z = 0.0;
        if (argc < 1 || !readVector3(ctx, argv[0], x, y, z))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        Level &level = api->levelOf(thisVal);
        const Vector3i position((int32_t) std::floor(x), (int32_t) std::floor(y), (int32_t) std::floor(z));
        if (position.y < level.getMinY() || position.y > level.getMaxY())
            return api->throwError("LocationOutOfWorldBoundariesError", "");
        // an unloaded chunk answers undefined, the way the game does
        if (level.peekBlockPtr(position.x, position.y, position.z) == nullptr)
            return JS_UNDEFINED;
        return api->makeBlock(level, position.x, position.y, position.z);
    }

    JSValue dimensionSetBlockType(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        double x = 0.0, y = 0.0, z = 0.0;
        if (argc < 2 || !readVector3(ctx, argv[0], x, y, z))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        Level &level = api->levelOf(thisVal);
        const int32_t blockY = (int32_t) std::floor(y);
        if (blockY < level.getMinY() || blockY > level.getMaxY())
            return api->throwError("LocationOutOfWorldBoundariesError", "");
        api->setBlockType(level, (int32_t) std::floor(x), blockY, (int32_t) std::floor(z),
                          typeIdentifier(ctx, argv[1]));
        return JS_UNDEFINED;
    }

    JSValue dimensionRunCommand(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const int32_t success = api->runCommandAsConsole(argc >= 1 ? toString(ctx, argv[0]) : std::string());
        JSValue result = api->makeInstance("CommandResult", new ScriptNative());
        JS_DefinePropertyValueStr(ctx, result, "successCount", JS_NewInt32(ctx, success), JS_PROP_ENUMERABLE);
        return result;
    }

    JSValue dimensionGetPlayers(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const Level &level = api->levelOf(thisVal);
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (auto &entry: api->host().getPlayers()) {
            if (entry.second.isSpawned() && entry.second.isIn(level))
                JS_SetPropertyUint32(ctx, array, index++, api->makePlayer(entry.second));
        }
        return array;
    }

    JSValue dimensionSpawnEntity(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        double x = 0.0, y = 0.0, z = 0.0;
        if (argc < 2 || !readVector3(ctx, argv[1], x, y, z))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        ServerActor *actor = api->host().spawnActor(api->levelOf(thisVal), typeIdentifier(ctx, argv[0]),
                                                    Vector3f((float) x, (float) y, (float) z));
        if (actor == nullptr)
            return api->throwError("EntitySpawnError", "");
        return api->makeActor(*actor);
    }

    JSValue dimensionSpawnItem(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptItemRef *item = argc >= 1 ? api->native<ScriptItemRef>(argv[0]) : nullptr;
        double x = 0.0, y = 0.0, z = 0.0;
        if (item == nullptr || argc < 2 || !readVector3(ctx, argv[1], x, y, z))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        api->host().spawnItemActor(api->levelOf(thisVal), item->mItem->mTypeId, item->mItem->mAmount,
                                   Vector3f((float) x, (float) y, (float) z));
        return JS_UNDEFINED;
    }

    JSValue dimensionSpawnParticle(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        double x = 0.0, y = 0.0, z = 0.0;
        if (argc < 2 || !readVector3(ctx, argv[1], x, y, z))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        api->host().spawnParticleEffect(api->levelOf(thisVal), toString(ctx, argv[0]),
                                        Vector3f((float) x, (float) y, (float) z));
        return JS_UNDEFINED;
    }

    JSValue dimensionPlaySound(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        double x = 0.0, y = 0.0, z = 0.0;
        if (argc < 2 || !readVector3(ctx, argv[1], x, y, z))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        float volume = 1.0f;
        float pitch = 1.0f;
        if (argc >= 3 && JS_IsObject(argv[2])) {
            JSValue volumeValue = JS_GetPropertyStr(ctx, argv[2], "volume");
            JSValue pitchValue = JS_GetPropertyStr(ctx, argv[2], "pitch");
            double parsed = 1.0;
            if (!JS_IsUndefined(volumeValue) && JS_ToFloat64(ctx, &parsed, volumeValue) == 0)
                volume = (float) parsed;
            if (!JS_IsUndefined(pitchValue) && JS_ToFloat64(ctx, &parsed, pitchValue) == 0)
                pitch = (float) parsed;
            JS_FreeValue(ctx, volumeValue);
            JS_FreeValue(ctx, pitchValue);
        }

        api->host().playNamedSound(api->levelOf(thisVal), toString(ctx, argv[0]),
                                   Vector3f((float) x, (float) y, (float) z), volume, pitch);
        return JS_UNDEFINED;
    }

    bool withinDistance(const Vector3f &position, double centerX, double centerY, double centerZ,
                        double maxDistance) {
        if (maxDistance < 0.0)
            return true;
        const double dx = position.x - centerX;
        const double dy = position.y - centerY;
        const double dz = position.z - centerZ;
        return dx * dx + dy * dy + dz * dz <= maxDistance * maxDistance;
    }

    JSValue dimensionGetEntities(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const Level &level = api->levelOf(thisVal);

        double centerX = 0.0, centerY = 0.0, centerZ = 0.0;
        double maxDistance = -1.0;
        const bool hasOptions = argc >= 1 && JS_IsObject(argv[0]);
        if (hasOptions) {
            JSValue location = JS_GetPropertyStr(ctx, argv[0], "location");
            readVector3(ctx, location, centerX, centerY, centerZ);
            JS_FreeValue(ctx, location);

            JSValue maxDistanceValue = JS_GetPropertyStr(ctx, argv[0], "maxDistance");
            if (!JS_IsUndefined(maxDistanceValue))
                JS_ToFloat64(ctx, &maxDistance, maxDistanceValue);
            JS_FreeValue(ctx, maxDistanceValue);
        }

        JSAtom matchesAtom = JS_NewAtom(ctx, "matches");
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        const auto consider = [&](JSValue candidate, const Vector3f &position) {
            bool keep = withinDistance(position, centerX, centerY, centerZ, maxDistance);
            if (keep && hasOptions) {
                JSValue matches = JS_Invoke(ctx, candidate, matchesAtom, 1, argv);
                keep = JS_ToBool(ctx, matches) == 1;
                JS_FreeValue(ctx, matches);
            }
            if (keep)
                JS_SetPropertyUint32(ctx, array, index++, candidate);
            else
                JS_FreeValue(ctx, candidate);
        };

        for (auto &entry: api->host().getActors()) {
            ServerActor &actor = *entry.second;
            if (actor.isAlive() && actor.isIn(level))
                consider(api->makeActor(actor), actor.getPosition());
        }
        for (auto &entry: api->host().getPlayers()) {
            ServerPlayer &player = entry.second;
            if (player.isSpawned() && player.isIn(level))
                consider(api->makePlayer(player), player.getPosition());
        }
        JS_FreeAtom(ctx, matchesAtom);
        return array;
    }
}

void ScriptApi::_bindSystem() {
    JSValue system = defineClass("System");
    methodMagic(system, "System", "run", systemRun, 1, 0);
    methodMagic(system, "System", "runTimeout", systemRun, 2, 1);
    methodMagic(system, "System", "runInterval", systemRun, 2, 2);
    method(system, "System", "clearRun", systemClearRun, 1);
    property(system, "System", "currentTick", systemGetCurrentTick);
    propertyMagic(system, "System", "afterEvents", holderGetter, 2);
    propertyMagic(system, "System", "beforeEvents", holderGetter, 3);

    mSystem = makeInstance("System", new ScriptNative());
    if (mSurface.has("system"))
        mServerExports.emplace_back("system", JS_DupValue(mContext, mSystem));
}

void ScriptApi::_bindWorld() {
    JSValue world = defineClass("World");
    method(world, "World", "sendMessage", worldSendMessage, 1);
    method(world, "World", "getAllPlayers", worldGetAllPlayers, 0);
    method(world, "World", "getDimension", worldGetDimension, 1);
    method(world, "World", "getEntity", worldGetEntity, 1);
    method(world, "World", "getDynamicProperty", worldGetDynamicProperty, 1);
    method(world, "World", "setDynamicProperty", worldSetDynamicProperty, 2);
    method(world, "World", "getDynamicPropertyIds", worldGetDynamicPropertyIds, 0);
    method(world, "World", "clearDynamicProperties", worldClearDynamicProperties, 0);
    propertyMagic(world, "World", "afterEvents", holderGetter, 0);
    propertyMagic(world, "World", "beforeEvents", holderGetter, 1);
    propertyMagic(world, "World", "scoreboard", holderGetter, 4);
    propertyMagic(world, "World", "tickingAreaManager", holderGetter, 5);

    mWorld = makeInstance("World", new ScriptNative());
    if (mSurface.has("world"))
        mServerExports.emplace_back("world", JS_DupValue(mContext, mWorld));
}

void ScriptApi::_bindBlock() {
    JSValue block = defineClass("Block");
    method(block, "Block", "setType", blockSetType, 1);
    method(block, "Block", "setPermutation", blockSetPermutation, 1);
    method(block, "Block", "center", blockCenter, 0);
    methodMagic(block, "Block", "above", blockOffset, 1, 0);
    methodMagic(block, "Block", "below", blockOffset, 1, 1);
    methodMagic(block, "Block", "north", blockOffset, 1, 2);
    methodMagic(block, "Block", "south", blockOffset, 1, 3);
    methodMagic(block, "Block", "east", blockOffset, 1, 4);
    methodMagic(block, "Block", "west", blockOffset, 1, 5);
    method(block, "Block", "matches", blockMatches, 2);
    property(block, "Block", "location", blockGetLocation);
    property(block, "Block", "dimension", blockGetDimension);
    property(block, "Block", "isValid", blockGetIsValid);
    propertyMagic(block, "Block", "x", blockGetCoord, 0);
    propertyMagic(block, "Block", "y", blockGetCoord, 1);
    propertyMagic(block, "Block", "z", blockGetCoord, 2);
    property(block, "Block", "typeId", blockGetTypeId);
    property(block, "Block", "isAir", blockGetIsAir);
    property(block, "Block", "permutation", blockGetPermutation);

    defineClass("BlockVolume", blockVolumeConstructor, 2);
    defineClass("EntityWaypoint", entityWaypointConstructor, 3);
}

void ScriptApi::_bindItemStack() {
    JSValue item = defineClass("ItemStack", itemStackConstructor, 2);
    property(item, "ItemStack", "typeId", itemStackGetTypeId);
    property(item, "ItemStack", "amount", itemStackGetAmount, itemStackSetAmount);
    method(item, "ItemStack", "getDynamicProperty", itemStackGetDynamicProperty, 1);
    method(item, "ItemStack", "setDynamicProperty", itemStackSetDynamicProperty, 2);
    method(item, "ItemStack", "getDynamicPropertyIds", itemStackGetDynamicPropertyIds, 0);
    method(item, "ItemStack", "getComponent", itemStackGetComponent, 1);
    method(item, "ItemStack", "hasComponent", itemStackHasComponent, 1);
    method(item, "ItemStack", "getTags", itemStackGetTags, 0);
    method(item, "ItemStack", "hasTag", itemStackHasTag, 1);

    JSValue durability = defineClass("ItemDurabilityComponent");
    property(durability, "ItemDurabilityComponent", "damage", durabilityGetDamage, durabilitySetDamage);
    property(durability, "ItemDurabilityComponent", "maxDurability", durabilityGetMax);
    method(durability, "ItemDurabilityComponent", "getDamageChance", durabilityGetDamageChance, 1);

    JSValue enchantable = defineClass("ItemEnchantableComponent");
    method(enchantable, "ItemEnchantableComponent", "getEnchantment", enchantableGetEnchantment, 1);

    JSValue cooldown = defineClass("ItemCooldownComponent");
    property(cooldown, "ItemCooldownComponent", "cooldownTicks", cooldownGetTicks);
    property(cooldown, "ItemCooldownComponent", "cooldownCategory", cooldownGetCategory);
}

void ScriptApi::_bindDimension() {
    JSValue dimension = defineClass("Dimension");
    property(dimension, "Dimension", "id", dimensionGetId);
    method(dimension, "Dimension", "getBlock", dimensionGetBlock, 1);
    method(dimension, "Dimension", "setBlockType", dimensionSetBlockType, 2);
    method(dimension, "Dimension", "runCommand", dimensionRunCommand, 1);
    method(dimension, "Dimension", "getPlayers", dimensionGetPlayers, 1);
    method(dimension, "Dimension", "spawnEntity", dimensionSpawnEntity, 3);
    method(dimension, "Dimension", "spawnItem", dimensionSpawnItem, 2);
    method(dimension, "Dimension", "spawnParticle", dimensionSpawnParticle, 3);
    method(dimension, "Dimension", "playSound", dimensionPlaySound, 3);
    method(dimension, "Dimension", "getEntities", dimensionGetEntities, 1);
}
