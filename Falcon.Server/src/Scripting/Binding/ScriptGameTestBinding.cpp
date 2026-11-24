#include "Scripting/Binding/ScriptApi.h"

#include "Core/Debug/BedrockLog.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/Binding/ScriptGameTestBootstrap.h"
#include "Scripting/Binding/ScriptSimulatedPlayerBinding.h"
#include "Scripting/ScriptEngine.h"
#include "World/Actor/Actor.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/AI/Navigation/Path.h"
#include "World/Actor/AI/Navigation/PathFinder.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Misc/PrimedTntActor.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Level/Block/Actor/CauldronBlockActor.h"
#include "World/Level/Block/Actor/SculkCatalystBlockActor.h"
#include "World/Level/Block/CauldronBlock.h"
#include "World/Level/Block/ButtonBlock.h"
#include "World/Level/Block/LeverBlock.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

#include <cmath>
#include <cstring>

using namespace ScriptValues;

namespace {
    JSValue typeError(JSContext *ctx) {
        return JS_ThrowTypeError(ctx, "Native type conversion failed.");
    }

    void addFunction(JSContext *ctx, JSValue object, const char *name, JSCFunction *function, int length) {
        JS_SetPropertyStr(ctx, object, name, JS_NewCFunction(ctx, function, name, length));
    }

    int32_t intOf(JSContext *ctx, JSValueConst value) {
        int32_t result = 0;
        JS_ToInt32(ctx, &result, value);
        return result;
    }

    double numberOf(JSContext *ctx, JSValueConst value) {
        double result = 0.0;
        JS_ToFloat64(ctx, &result, value);
        return result;
    }

    std::string withNamespace(const std::string &id) {
        return id.find(':') == std::string::npos ? "minecraft:" + id : id;
    }

    // the collision box at the feet, as the game sizes the actor
    void boxOf(Actor &actor, Vector3f &low, Vector3f &high) {
        float width = MovementPhysics::PLAYER_WIDTH, height = MovementPhysics::PLAYER_HEIGHT;
        if (dynamic_cast<ServerPlayer *>(&actor) == nullptr) {
            const ActorSize size = ActorClassRegistry::getSize(actor.getIdentifier());
            width = size.mWidth;
            height = size.mHeight;
        }
        const Vector3f &position = actor.getPosition();
        low = Vector3f(position.x - width * 0.5f, position.y, position.z - width * 0.5f);
        high = Vector3f(position.x + width * 0.5f, position.y + height, position.z + width * 0.5f);
    }

    // every live entity of the level, players included
    template <typename Visit>
    void forEachEntity(ScriptApi &api, Level &level, Visit visit) {
        ServerNetworkHandler &host = api.host();
        for (auto &entry: host.getActors()) {
            ServerActor &actor = *entry.second;
            if (actor.isAlive() && actor.isIn(level))
                visit(static_cast<Actor &>(actor), api.makeActor(actor));
        }
        for (std::unique_ptr<ItemActor> &item: host.getItemEntities()) {
            if (item != nullptr && item->isAlive() && item->isIn(level))
                visit(static_cast<Actor &>(*item), api.makeEntity(*item));
        }
        for (auto &entry: host.getPlayers()) {
            ServerPlayer &player = entry.second;
            if (player.isSpawned() && player.isIn(level))
                visit(static_cast<Actor &>(player), api.makePlayer(player));
        }
    }

    JSValue gtPrivilege(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return JS_NewInt32(ctx, (int32_t) ScriptApi::fromContext(ctx)->privilege());
    }

    JSValue gtLog(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc >= 1)
            LOG_ERROR(LogAreaID::Script, "[%s] %s", api->packName().c_str(), toString(ctx, argv[0]).c_str());
        return JS_UNDEFINED;
    }

    // Test.print: every player reads it in chat
    JSValue gtSay(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        if (argc >= 1)
            ScriptApi::fromContext(ctx)->host().broadcastSystemMessage(toString(ctx, argv[0]));
        return JS_UNDEFINED;
    }

    JSValue gtIsDimensionId(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const std::string id = withNamespace(argc >= 1 ? toString(ctx, argv[0]) : std::string());
        if (id == "minecraft:overworld" || id == "minecraft:nether" || id == "minecraft:the_end")
            return JS_TRUE;
        bool custom = false;
        const DimensionType dimension = Dimension::findCustom(id, custom);
        return JS_NewBool(ctx, custom && api->host().getWorlds().getDefault().hasLevel(dimension));
    }

    // (id, failure): failure undefined when the test passed
    JSValue gtFinished(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1)
            return JS_UNDEFINED;
        const int32_t id = intOf(ctx, argv[0]);
        if (argc < 2 || JS_IsUndefined(argv[1])) {
            api->host().getScriptEngine().gameTests().finished(id, nullptr);
            return JS_UNDEFINED;
        }

        GameTestRunner::Failure failure;
        JSValue message = JS_GetPropertyStr(ctx, argv[1], "message");
        failure.mMessage = toString(ctx, message);
        JS_FreeValue(ctx, message);
        JSValue parameters = JS_GetPropertyStr(ctx, argv[1], "parameters");
        JSValue length = JS_GetPropertyStr(ctx, parameters, "length");
        const int32_t count = JS_IsNumber(length) ? intOf(ctx, length) : 0;
        JS_FreeValue(ctx, length);
        for (int32_t index = 0; index < count; ++index) {
            JSValue parameter = JS_GetPropertyUint32(ctx, parameters, (uint32_t) index);
            failure.mParameters.push_back(toString(ctx, parameter));
            JS_FreeValue(ctx, parameter);
        }
        JS_FreeValue(ctx, parameters);
        JSValue context = JS_GetPropertyStr(ctx, argv[1], "context");
        if (JS_IsObject(context)) {
            JSValue absolute = JS_GetPropertyStr(ctx, context, "absolutePosition");
            double x = 0.0, y = 0.0, z = 0.0;
            if (readVector3(ctx, absolute, x, y, z)) {
                failure.mHasPosition = true;
                failure.mPosition = Vector3i((int32_t) std::floor(x), (int32_t) std::floor(y), (int32_t) std::floor(z));
            }
            JS_FreeValue(ctx, absolute);
        }
        JS_FreeValue(ctx, context);
        api->host().getScriptEngine().gameTests().finished(id, &failure);
        return JS_UNDEFINED;
    }

    // (dimension, low xyz, high xyz, typeId): entities whose box overlaps [low, high)
    JSValue gtEntitiesIn(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 8)
            return typeError(ctx);
        Level &level = api->levelOf(argv[0]);
        const Vector3f low((float) numberOf(ctx, argv[1]), (float) numberOf(ctx, argv[2]),
                           (float) numberOf(ctx, argv[3]));
        const Vector3f high((float) numberOf(ctx, argv[4]), (float) numberOf(ctx, argv[5]),
                            (float) numberOf(ctx, argv[6]));
        const std::string type = toString(ctx, argv[7]);
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        forEachEntity(*api, level, [&](Actor &actor, JSValue value) {
            Vector3f boxLow, boxHigh;
            boxOf(actor, boxLow, boxHigh);
            const bool overlaps = boxLow.x < high.x && boxHigh.x > low.x && boxLow.y < high.y && boxHigh.y > low.y
                                  && boxLow.z < high.z && boxHigh.z > low.z;
            if (overlaps && (type.empty() || type == actor.getIdentifier()))
                JS_SetPropertyUint32(ctx, array, index++, value);
            else
                JS_FreeValue(ctx, value);
        });
        return array;
    }

    // (dimension, xyz, typeId): entities whose box holds the point
    JSValue gtEntitiesTouching(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 5)
            return typeError(ctx);
        Level &level = api->levelOf(argv[0]);
        const Vector3f point((float) numberOf(ctx, argv[1]), (float) numberOf(ctx, argv[2]),
                             (float) numberOf(ctx, argv[3]));
        const std::string type = toString(ctx, argv[4]);
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        forEachEntity(*api, level, [&](Actor &actor, JSValue value) {
            Vector3f low, high;
            boxOf(actor, low, high);
            const bool inside = point.x >= low.x && point.x <= high.x && point.y >= low.y && point.y <= high.y
                                && point.z >= low.z && point.z <= high.z;
            if (inside && (type.empty() || type == actor.getIdentifier()))
                JS_SetPropertyUint32(ctx, array, index++, value);
            else
                JS_FreeValue(ctx, value);
        });
        return array;
    }

    MobActor *mobOf(ScriptApi &api, JSValueConst value) {
        return dynamic_cast<MobActor *>(api.resolveEntity(value));
    }

    // (mob, xyz): undefined for anything but a mob
    JSValue gtCanReach(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        MobActor *mob = argc >= 4 ? mobOf(*api, argv[0]) : nullptr;
        if (mob == nullptr)
            return JS_UNDEFINED;
        Level &level = api->host().getLevelFor(*mob);
        const Vector3f target((float) numberOf(ctx, argv[1]) + 0.5f, (float) numberOf(ctx, argv[2]),
                              (float) numberOf(ctx, argv[3]) + 0.5f);
        Path path;
        const bool found = PathFinder::get().findPath(level, *mob, target, mob->getNavigation().getOptions(), path);
        return JS_NewBool(ctx, found && path.reachesTarget());
    }

    // (mob, xyz, speed): false when the entity cannot be told to move
    JSValue gtWalkTo(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        MobActor *mob = argc >= 5 ? mobOf(*api, argv[0]) : nullptr;
        if (mob == nullptr)
            return JS_FALSE;
        mob->getNavigation().moveTo(Vector3f((float) numberOf(ctx, argv[1]), (float) numberOf(ctx, argv[2]),
                                             (float) numberOf(ctx, argv[3])),
                                    (float) numberOf(ctx, argv[4]));
        return JS_TRUE;
    }

    // (entity, armor slot, item name, data): ArmorSlot counts from the head
    JSValue gtHasArmor(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        Actor *entity = argc >= 4 ? api->resolveEntity(argv[0]) : nullptr;
        if (entity == nullptr)
            return JS_FALSE;
        const int32_t slot = intOf(ctx, argv[1]);
        const std::string name = toString(ctx, argv[2]);
        const int32_t data = intOf(ctx, argv[3]);
        const ItemStack *item = nullptr;
        if (ServerPlayer *player = dynamic_cast<ServerPlayer *>(entity)) {
            if (slot >= 0 && slot < (int32_t) player->getInventory().getArmorContents().size())
                item = &player->getInventory().getArmor(slot);
        } else if (MobActor *mob = dynamic_cast<MobActor *>(entity)) {
            if (slot >= 0 && MobEquipment::HEAD + slot < MobEquipment::SLOT_COUNT)
                item = &mob->getEquipment().getSlot(MobEquipment::HEAD + slot);
        }
        return JS_NewBool(ctx, item != nullptr && !item->isAir() && item->mDefinition->getIdentifier() == name
                                       && item->mDamage == data);
    }

    // an item entity's stack as { typeId, amount }, undefined for anything else
    JSValue gtItemOf(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ItemActor *item = argc >= 1 ? dynamic_cast<ItemActor *>(api->resolveEntity(argv[0])) : nullptr;
        if (item == nullptr || item->getItem().isAir())
            return JS_UNDEFINED;
        JSValue result = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, result, "typeId", string(ctx, item->getItem().mDefinition->getIdentifier()));
        JS_SetPropertyStr(ctx, result, "amount", JS_NewInt32(ctx, item->getItem().mCount));
        return result;
    }

    // Test.destroyBlock: broken as a player would without a tool, drops when asked
    JSValue gtDestroy(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 5)
            return typeError(ctx);
        Level &level = api->levelOf(argv[0]);
        const Vector3i position(intOf(ctx, argv[1]), intOf(ctx, argv[2]), intOf(ctx, argv[3]));
        const BlockState broken = level.getBlockState(position.x, position.y, position.z);
        if (broken.mName == "minecraft:air")
            return JS_UNDEFINED;
        BlockActionHandler::destroyBlock(api->host(), level, position, broken, JS_ToBool(ctx, argv[4]) == 1,
                                         ItemStack::air());
        return JS_UNDEFINED;
    }

    // Test.killAllEntities
    JSValue gtKill(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        Actor *entity = argc >= 1 ? api->resolveEntity(argv[0]) : nullptr;
        if (entity != nullptr && dynamic_cast<ServerPlayer *>(entity) == nullptr)
            entity->kill();
        return JS_UNDEFINED;
    }

    JSValue gtPressButton(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 4)
            return typeError(ctx);
        Level &level = api->levelOf(argv[0]);
        const Vector3i position(intOf(ctx, argv[1]), intOf(ctx, argv[2]), intOf(ctx, argv[3]));
        const BlockState state = level.getBlockState(position.x, position.y, position.z);
        if (!ButtonBlock::matches(state.mName))
            return JS_FALSE;
        const bool wasPressed = state.mStates.getBool("button_pressed_bit", false);
        RedstoneSystem::onButtonActivated(api->host(), level, position, state);
        if (!wasPressed)
            api->host().getScriptEngine().onButtonPush(level, position, nullptr);
        return JS_TRUE;
    }

    JSValue gtPullLever(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 4)
            return typeError(ctx);
        Level &level = api->levelOf(argv[0]);
        const Vector3i position(intOf(ctx, argv[1]), intOf(ctx, argv[2]), intOf(ctx, argv[3]));
        const BlockState state = level.getBlockState(position.x, position.y, position.z);
        if (!LeverBlock::matches(state.mName))
            return JS_FALSE;
        RedstoneSystem::onLeverActivated(api->host(), level, position, state);
        return JS_TRUE;
    }

    // (entity, ticks): 0 set, 1 no explode component, 2 a negative length
    JSValue gtSetFuse(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        Actor *entity = argc >= 2 ? api->resolveEntity(argv[0]) : nullptr;
        const int32_t ticks = argc >= 2 ? intOf(ctx, argv[1]) : 0;
        PrimedTntActor *tnt = dynamic_cast<PrimedTntActor *>(entity);
        MobActor *mob = dynamic_cast<MobActor *>(entity);
        const bool explodes = tnt != nullptr || (mob != nullptr && mob->getComponent("minecraft:explode") != nullptr);
        if (!explodes)
            return JS_NewInt32(ctx, 1);
        if (ticks < 0)
            return JS_NewInt32(ctx, 2);
        if (tnt != nullptr)
            tnt->setFuse(ticks);
        else
            mob->getExplode().setFuseLength(api->host(), *mob, ticks);
        return JS_NewInt32(ctx, 0);
    }

    // (dimension, typeId, xyz, behaviors): null when behaviors are off and the type is no mob, undefined when
    // nothing spawned
    JSValue gtSpawn(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 6)
            return typeError(ctx);
        Level &level = api->levelOf(argv[0]);
        const std::string type = withNamespace(toString(ctx, argv[1]));
        const bool behaviors = JS_ToBool(ctx, argv[5]) == 1;
        if (!behaviors && ActorClassRegistry::getMobPrototype(type) == nullptr)
            return JS_NULL;
        ServerNetworkHandler &host = api->host();
        ServerActor *actor = host.spawnActor(level, type,
                                             Vector3f((float) numberOf(ctx, argv[2]), (float) numberOf(ctx, argv[3]),
                                                      (float) numberOf(ctx, argv[4])),
                                             [&host, behaviors](ServerActor &spawned) {
                                                 MobActor *mob = dynamic_cast<MobActor *>(&spawned);
                                                 if (!behaviors && mob != nullptr)
                                                     mob->removeBehaviors(host);
                                             });
        return actor == nullptr ? JS_UNDEFINED : api->makeActor(*actor);
    }

    // (dimension, xyz, fluid): false when the block is no cauldron; the fluid as FluidType counts it, Water 0,
    // Lava 1, PowderSnow 2, Potion 3; the cauldron keeps its fill level
    JSValue gtSetCauldron(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 5)
            return typeError(ctx);
        Level &level = api->levelOf(argv[0]);
        const Vector3i position(intOf(ctx, argv[1]), intOf(ctx, argv[2]), intOf(ctx, argv[3]));
        const int32_t fluid = intOf(ctx, argv[4]);
        const BlockState state = level.getBlockState(position.x, position.y, position.z);
        if (!CauldronBlock::matches(state.mName))
            return JS_FALSE;
        const CauldronBlock::Liquid liquid = fluid == 1   ? CauldronBlock::Liquid::Lava
                                             : fluid == 2 ? CauldronBlock::Liquid::PowderSnow
                                                          : CauldronBlock::Liquid::Water;
        const int fill = CauldronBlock(state).getFillLevel();
        api->setBlock(level, position.x, position.y, position.z, CauldronBlock::makeState(liquid, fill));
        // a potion cauldron is water with a potion id on its block actor, any other liquid clears it
        CauldronBlockActor &actor = level.getBlockActors().getOrCreate<CauldronBlockActor>(position);
        Tag data = actor.saveNbt();
        data.putShort("PotionId", fluid == 3 ? 0 : -1);
        data.putShort("PotionType", fluid == 3 ? 0 : -1);
        actor.loadNbt(data, api->host().getCodecContext());
        BlockActionHandler::broadcastBlockActorData(api->host(), level, actor);
        return JS_TRUE;
    }

    SculkSpreader *spreaderAt(JSContext *ctx, JSValueConst *argv) {
        Level &level = ScriptApi::fromContext(ctx)->levelOf(argv[0]);
        SculkCatalystBlockActor *catalyst = level.getBlockActors().find<SculkCatalystBlockActor>(
                Vector3i(intOf(ctx, argv[1]), intOf(ctx, argv[2]), intOf(ctx, argv[3])));
        return catalyst == nullptr ? nullptr : &catalyst->getSpreader();
    }

    // (dimension, xyz): the cursors of the catalyst there, position and charge; undefined without a catalyst
    JSValue gtSculkCursors(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        if (argc < 4)
            return typeError(ctx);
        const SculkSpreader *spreader = spreaderAt(ctx, argv);
        if (spreader == nullptr)
            return JS_UNDEFINED;
        JSValue cursors = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const SculkChargeCursor &cursor: spreader->getCursors()) {
            JSValue entry = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, entry, "x", JS_NewInt32(ctx, cursor.mPosition.x));
            JS_SetPropertyStr(ctx, entry, "y", JS_NewInt32(ctx, cursor.mPosition.y));
            JS_SetPropertyStr(ctx, entry, "z", JS_NewInt32(ctx, cursor.mPosition.z));
            JS_SetPropertyStr(ctx, entry, "charge", JS_NewInt32(ctx, cursor.mCharge));
            JS_SetPropertyUint32(ctx, cursors, index++, entry);
        }
        return cursors;
    }

    // (dimension, xyz, cursor xyz, charge): nothing without a catalyst
    JSValue gtSculkAdd(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        if (argc < 8)
            return typeError(ctx);
        SculkSpreader *spreader = spreaderAt(ctx, argv);
        if (spreader != nullptr)
            spreader->addCursors(Vector3i(intOf(ctx, argv[4]), intOf(ctx, argv[5]), intOf(ctx, argv[6])),
                                 intOf(ctx, argv[7]));
        return JS_UNDEFINED;
    }

    JSValue runBootstrap(JSContext *ctx, const char *source, JSValue argument) {
        JSValue factory = JS_Eval(ctx, source, std::strlen(source), "<gametest>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(factory)) {
            JS_FreeValue(ctx, argument);
            return factory;
        }
        JSValue result = JS_Call(ctx, factory, JS_UNDEFINED, 1, &argument);
        JS_FreeValue(ctx, factory);
        JS_FreeValue(ctx, argument);
        return result;
    }

    JSValue gtFlags(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const std::string name = argc >= 1 ? toString(ctx, argv[0]) : std::string();
        // defined, not set: a setter a pack put on Object.prototype must not see or replace them
        JSValue flags = JS_NewObject(ctx);
        JS_DefinePropertyValueStr(ctx, flags, "early", JS_NewBool(ctx, api->gameTestSurface().isEarly(name)),
                                  JS_PROP_C_W_E);
        JS_DefinePropertyValueStr(ctx, flags, "blocked", JS_NewBool(ctx, api->gameTestSurface().isBlocked(name)),
                                  JS_PROP_C_W_E);
        return flags;
    }
}

void ScriptApi::_installGameTest() {
    JSValue native = JS_NewObject(mContext);
    addFunction(mContext, native, "privilege", gtPrivilege, 0);
    addFunction(mContext, native, "flags", gtFlags, 1);
    addFunction(mContext, native, "log", gtLog, 1);
    addFunction(mContext, native, "say", gtSay, 1);
    addFunction(mContext, native, "isDimensionId", gtIsDimensionId, 1);
    addFunction(mContext, native, "finished", gtFinished, 2);
    addFunction(mContext, native, "entitiesIn", gtEntitiesIn, 8);
    addFunction(mContext, native, "entitiesTouching", gtEntitiesTouching, 5);
    addFunction(mContext, native, "canReach", gtCanReach, 4);
    addFunction(mContext, native, "walkTo", gtWalkTo, 5);
    addFunction(mContext, native, "hasArmor", gtHasArmor, 4);
    addFunction(mContext, native, "itemOf", gtItemOf, 1);
    addFunction(mContext, native, "destroy", gtDestroy, 5);
    addFunction(mContext, native, "kill", gtKill, 1);
    addFunction(mContext, native, "pressButton", gtPressButton, 4);
    addFunction(mContext, native, "pullLever", gtPullLever, 4);
    addFunction(mContext, native, "setFuse", gtSetFuse, 2);
    addFunction(mContext, native, "spawn", gtSpawn, 6);
    addFunction(mContext, native, "setCauldron", gtSetCauldron, 5);
    addFunction(mContext, native, "sculkCursors", gtSculkCursors, 4);
    addFunction(mContext, native, "sculkAdd", gtSculkAdd, 8);
    ScriptSimulatedPlayerBinding::install(mContext, native);
    JS_SetPropertyStr(mContext, native, "errors", JS_DupValue(mContext, mErrors));
    JSValue server = JS_NewObject(mContext);
    for (const auto &entry: mServerExports)
        JS_SetPropertyStr(mContext, server, entry.first.c_str(), JS_DupValue(mContext, entry.second));
    JS_SetPropertyStr(mContext, native, "server", server);

    JSValue exports = runBootstrap(mContext, ScriptGameTestBootstrap::source(), native);
    if (JS_IsException(exports)) {
        logPendingException("<gametest>");
        return;
    }

    JSPropertyEnum *names = nullptr;
    uint32_t count = 0;
    if (JS_GetOwnPropertyNames(mContext, &names, &count, exports, JS_GPN_STRING_MASK) == 0) {
        for (uint32_t index = 0; index < count; ++index) {
            const char *name = JS_AtomToCString(mContext, names[index].atom);
            if (name != nullptr && mGameTestSurface.has(name))
                mGameTestExports.emplace_back(name, JS_GetProperty(mContext, exports, names[index].atom));
            if (name != nullptr)
                JS_FreeCString(mContext, name);
        }
        JS_FreePropertyEnum(mContext, names, count);
    }

    for (const char *name:
         {"GameTestErrorType", "GameTestCompletedErrorReason", "LookDuration", "PersonaArmSize", "PersonaPieceType"}) {
        if (!mGameTestSurface.has(name))
            continue;
        JSValue object = JS_NewObject(mContext);
        for (const auto &value: mGameTestSurface.enumValues(name))
            JS_DefinePropertyValueStr(mContext, object, value.first.c_str(), string(mContext, value.second),
                                      JS_PROP_ENUMERABLE);
        mGameTestExports.emplace_back(name, object);
    }

    mGameTestHooks = JS_GetPropertyStr(mContext, exports, "hooks");
    JS_FreeValue(mContext, exports);

    // each running test ticks once per server tick, like GameTestTicker
    JSContext *context = mContext;
    addTickJob([this, context]() {
        JSValue tick = JS_GetPropertyStr(context, mGameTestHooks, "tick");
        JSValue result = JS_Call(context, tick, mGameTestHooks, 0, nullptr);
        if (JS_IsException(result))
            logPendingException("<gametest>");
        JS_FreeValue(context, result);
        JS_FreeValue(context, tick);
        return true;
    });
}

namespace {
    JSValue callHook(JSContext *ctx, JSValueConst hooks, const char *name, int argc, JSValue *argv) {
        JSValue function = JS_GetPropertyStr(ctx, hooks, name);
        JSValue result = JS_Call(ctx, function, hooks, argc, argv);
        JS_FreeValue(ctx, function);
        for (int index = 0; index < argc; ++index)
            JS_FreeValue(ctx, argv[index]);
        return result;
    }

    int32_t intProperty(JSContext *ctx, JSValueConst object, const char *name, int32_t fallback) {
        JSValue value = JS_GetPropertyStr(ctx, object, name);
        int32_t result = fallback;
        if (JS_IsNumber(value))
            JS_ToInt32(ctx, &result, value);
        JS_FreeValue(ctx, value);
        return result;
    }

    bool boolProperty(JSContext *ctx, JSValueConst object, const char *name) {
        JSValue value = JS_GetPropertyStr(ctx, object, name);
        const bool result = JS_ToBool(ctx, value) == 1;
        JS_FreeValue(ctx, value);
        return result;
    }

    std::string stringProperty(JSContext *ctx, JSValueConst object, const char *name) {
        JSValue value = JS_GetPropertyStr(ctx, object, name);
        std::string result = JS_IsString(value) ? toString(ctx, value) : std::string();
        JS_FreeValue(ctx, value);
        return result;
    }

    std::vector<std::string> stringArray(JSContext *ctx, JSValueConst array) {
        std::vector<std::string> result;
        JSValue length = JS_GetPropertyStr(ctx, array, "length");
        int32_t count = 0;
        if (JS_IsNumber(length))
            JS_ToInt32(ctx, &count, length);
        JS_FreeValue(ctx, length);
        for (int32_t index = 0; index < count; ++index) {
            JSValue item = JS_GetPropertyUint32(ctx, array, (uint32_t) index);
            result.push_back(toString(ctx, item));
            JS_FreeValue(ctx, item);
        }
        return result;
    }
}

std::vector<GameTestRunner::Descriptor> ScriptApi::gameTestList() {
    std::vector<GameTestRunner::Descriptor> result;
    if (!JS_IsObject(mGameTestHooks))
        return result;
    JSValue list = callHook(mContext, mGameTestHooks, "list", 0, nullptr);
    if (JS_IsException(list)) {
        logPendingException("<gametest>");
        return result;
    }
    JSValue length = JS_GetPropertyStr(mContext, list, "length");
    int32_t count = 0;
    JS_ToInt32(mContext, &count, length);
    JS_FreeValue(mContext, length);
    for (int32_t index = 0; index < count; ++index) {
        JSValue entry = JS_GetPropertyUint32(mContext, list, (uint32_t) index);
        GameTestRunner::Descriptor descriptor;
        descriptor.mApi = this;
        descriptor.mName = stringProperty(mContext, entry, "name");
        descriptor.mStructureName = stringProperty(mContext, entry, "structureName");
        descriptor.mBatch = stringProperty(mContext, entry, "batch");
        JSValue tags = JS_GetPropertyStr(mContext, entry, "tags");
        descriptor.mTags = stringArray(mContext, tags);
        JS_FreeValue(mContext, tags);
        descriptor.mMaxTicks = intProperty(mContext, entry, "maxTicks", 100);
        descriptor.mSetupTicks = intProperty(mContext, entry, "setupTicks", 0);
        descriptor.mPadding = intProperty(mContext, entry, "padding", 1);
        descriptor.mRequired = boolProperty(mContext, entry, "required");
        descriptor.mMaxAttempts = intProperty(mContext, entry, "maxAttempts", 1);
        descriptor.mRequiredSuccessfulAttempts = intProperty(mContext, entry, "requiredSuccessfulAttempts", 1);
        descriptor.mRotateTest = boolProperty(mContext, entry, "rotateTest");
        JSValue location = JS_GetPropertyStr(mContext, entry, "structureLocation");
        double x = 0.0, y = 0.0, z = 0.0;
        if (JS_IsObject(location) && readVector3(mContext, location, x, y, z)) {
            descriptor.mHasStructureLocation = true;
            descriptor.mStructureLocation = Vector3i((int32_t) std::floor(x), (int32_t) std::floor(y),
                                                     (int32_t) std::floor(z));
        }
        JS_FreeValue(mContext, location);
        descriptor.mStructureDimension = stringProperty(mContext, entry, "structureDimension");
        JS_FreeValue(mContext, entry);
        result.push_back(std::move(descriptor));
    }
    JS_FreeValue(mContext, list);
    return result;
}

std::vector<std::string> ScriptApi::gameTestTags() {
    if (!JS_IsObject(mGameTestHooks))
        return {};
    JSValue tags = callHook(mContext, mGameTestHooks, "tags", 0, nullptr);
    if (JS_IsException(tags)) {
        logPendingException("<gametest>");
        return {};
    }
    std::vector<std::string> result = stringArray(mContext, tags);
    JS_FreeValue(mContext, tags);
    return result;
}

bool ScriptApi::gameTestStart(int32_t id, const std::string &name, const Vector3i &structureBlock, int32_t rotation,
                              const Vector3i &size, const Level &level) {
    if (!JS_IsObject(mGameTestHooks))
        return false;
    JSValue args[6] = {JS_NewInt32(mContext, id),
                       string(mContext, name),
                       vector3(mContext, (float) structureBlock.x, (float) structureBlock.y, (float) structureBlock.z),
                       JS_NewInt32(mContext, rotation),
                       vector3(mContext, (float) size.x, (float) size.y, (float) size.z),
                       makeDimension(level)};
    JSValue started = callHook(mContext, mGameTestHooks, "start", 6, args);
    if (JS_IsException(started)) {
        logPendingException("<gametest>");
        return false;
    }
    const bool result = JS_ToBool(mContext, started) == 1;
    JS_FreeValue(mContext, started);
    return result;
}

void ScriptApi::gameTestStop(int32_t id) {
    if (!JS_IsObject(mGameTestHooks))
        return;
    JSValue args[1] = {JS_NewInt32(mContext, id)};
    JSValue result = callHook(mContext, mGameTestHooks, "stop", 1, args);
    if (JS_IsException(result))
        logPendingException("<gametest>");
    JS_FreeValue(mContext, result);
}

void ScriptApi::gameTestBatch(bool before, const std::string &batchName) {
    if (!JS_IsObject(mGameTestHooks))
        return;
    JSValue args[2] = {string(mContext, before ? "before" : "after"), string(mContext, batchName)};
    JSValue result = callHook(mContext, mGameTestHooks, "batch", 2, args);
    if (JS_IsException(result))
        logPendingException("<gametest>");
    JS_FreeValue(mContext, result);
}
