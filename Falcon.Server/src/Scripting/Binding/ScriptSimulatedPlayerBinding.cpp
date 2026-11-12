#include "Scripting/Binding/ScriptSimulatedPlayerBinding.h"

#include "Network/ServerNetworkHandler.h"
#include "Scripting/Binding/ScriptApi.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "World/Actor/Player/ServerPlayer.h"

using namespace ScriptValues;

namespace {
    using LookDuration = SimulatedPlayerController::LookDuration;

    void addFunction(JSContext *ctx, JSValue object, const char *name, JSCFunction *function, int length) {
        JS_SetPropertyStr(ctx, object, name, JS_NewCFunction(ctx, function, name, length));
    }

    float floatOf(JSContext *ctx, JSValueConst value) {
        double result = 0.0;
        JS_ToFloat64(ctx, &result, value);
        return (float) result;
    }

    // a live simulated player, or null for anything else
    ServerPlayer *simulatedOf(ScriptApi &api, JSValueConst value) {
        ServerPlayer *player = api.resolvePlayer(value);
        return player != nullptr && player->isSimulated() ? player : nullptr;
    }

    LookDuration durationOf(JSContext *ctx, JSValueConst value) {
        int32_t duration = 2;
        JS_ToInt32(ctx, &duration, value);
        return duration == 0 ? LookDuration::Instant
                             : (duration == 1 ? LookDuration::Continuous : LookDuration::UntilMove);
    }

    // (dimension, x, y, z, yaw, name, game mode, test rotation)
    JSValue spSpawn(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 8)
            return JS_UNDEFINED;
        Level &level = api->levelOf(argv[0]);
        const Vector3f position(floatOf(ctx, argv[1]), floatOf(ctx, argv[2]), floatOf(ctx, argv[3]));
        int32_t gameType = 0, rotation = 0;
        JS_ToInt32(ctx, &gameType, argv[6]);
        JS_ToInt32(ctx, &rotation, argv[7]);
        ServerPlayer &player = api->host().spawnSimulatedPlayer(toString(ctx, argv[5]), level, position,
                                                                floatOf(ctx, argv[4]), gameType);
        player.getSimulatedController()->setTestRotation(rotation);
        return api->makePlayer(player);
    }

    JSValue spRemove(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 1 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr)
            return JS_FALSE;
        api->host().removeSimulatedPlayer(player->getNetworkIdentifier());
        return JS_TRUE;
    }

    JSValue spIs(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        return JS_NewBool(ctx, argc >= 1 && simulatedOf(*api, argv[0]) != nullptr);
    }

    // the test's rotation, -1 once the player is gone
    JSValue spRotation(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 1 ? simulatedOf(*api, argv[0]) : nullptr;
        return JS_NewInt32(ctx, player == nullptr ? -1 : player->getSimulatedController()->getTestRotation());
    }

    // (player, x, y, z, world)
    JSValue spMove(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 5 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr)
            return JS_FALSE;
        player->getSimulatedController()->moveInDirection(
                Vector3f(floatOf(ctx, argv[1]), floatOf(ctx, argv[2]), floatOf(ctx, argv[3])),
                JS_ToBool(ctx, argv[4]) == 1);
        return JS_TRUE;
    }

    // (player, x, y, z, faceTarget, speed)
    JSValue spMoveTo(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 6 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr)
            return JS_FALSE;
        player->getSimulatedController()->moveToLocation(
                Vector3f(floatOf(ctx, argv[1]), floatOf(ctx, argv[2]), floatOf(ctx, argv[3])),
                JS_ToBool(ctx, argv[4]) == 1, floatOf(ctx, argv[5]));
        return JS_TRUE;
    }

    JSValue spStop(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 1 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr)
            return JS_FALSE;
        player->getSimulatedController()->stopMoving();
        return JS_TRUE;
    }

    JSValue spJump(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 1 ? simulatedOf(*api, argv[0]) : nullptr;
        return JS_NewBool(ctx, player != nullptr && player->getSimulatedController()->jump(*player));
    }

    // (player) the sprint flag, (player, value) sets the order
    JSValue spSprint(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 1 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr)
            return JS_UNDEFINED;
        if (argc >= 2)
            player->getSimulatedController()->setSprinting(JS_ToBool(ctx, argv[1]) == 1);
        return JS_NewBool(ctx, player->getFlags().get(ActorFlag::Sprinting));
    }

    // (player, x, y, z, duration)
    JSValue spLook(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 5 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr)
            return JS_FALSE;
        player->getSimulatedController()->lookAtLocation(
                *player, Vector3f(floatOf(ctx, argv[1]), floatOf(ctx, argv[2]), floatOf(ctx, argv[3])),
                durationOf(ctx, argv[4]));
        return JS_TRUE;
    }

    // (player, entity, duration): false when the entity is gone
    JSValue spLookEntity(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 3 ? simulatedOf(*api, argv[0]) : nullptr;
        Actor *target = argc >= 3 ? api->resolveEntity(argv[1]) : nullptr;
        if (player == nullptr || target == nullptr)
            return JS_FALSE;
        Vector3f eyes = target->getPosition();
        if (dynamic_cast<ServerPlayer *>(target) != nullptr)
            eyes.y += MovementPhysics::PLAYER_EYE_HEIGHT;
        player->getSimulatedController()->lookAtEntity(*player, target->getUniqueId(), eyes, durationOf(ctx, argv[2]));
        return JS_TRUE;
    }

    // (player, yaw): absolute, the script side adds the current yaw or the test's turn
    JSValue spYaw(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 2 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr)
            return JS_FALSE;
        player->getSimulatedController()->setBodyRotation(*player, floatOf(ctx, argv[1]));
        return JS_TRUE;
    }

    // { x: pitch, y: head yaw, body: yaw }
    JSValue spRotationOf(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 1 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr)
            return JS_UNDEFINED;
        const Vector3f &rotation = player->getRotation();
        JSValue result = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, result, "x", JS_NewFloat64(ctx, rotation.x));
        JS_SetPropertyStr(ctx, result, "y", JS_NewFloat64(ctx, rotation.z));
        JS_SetPropertyStr(ctx, result, "body", JS_NewFloat64(ctx, rotation.y));
        return result;
    }

    JSValue spGameMode(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        return JS_NewInt32(ctx, argc >= 1 ? api->parseGameMode(toString(ctx, argv[0])) : -1);
    }

    JSValue spPrototype(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        if (argc >= 1)
            ScriptApi::fromContext(ctx)->setSimulatedPlayerPrototype(argv[0]);
        return JS_UNDEFINED;
    }
}

namespace ScriptSimulatedPlayerBinding {
    void install(JSContext *ctx, JSValue native) {
        JSValue sp = JS_NewObject(ctx);
        addFunction(ctx, sp, "spawn", spSpawn, 8);
        addFunction(ctx, sp, "remove", spRemove, 1);
        addFunction(ctx, sp, "is", spIs, 1);
        addFunction(ctx, sp, "rotation", spRotation, 1);
        addFunction(ctx, sp, "move", spMove, 5);
        addFunction(ctx, sp, "moveTo", spMoveTo, 6);
        addFunction(ctx, sp, "stop", spStop, 1);
        addFunction(ctx, sp, "jump", spJump, 1);
        addFunction(ctx, sp, "sprint", spSprint, 2);
        addFunction(ctx, sp, "look", spLook, 5);
        addFunction(ctx, sp, "lookEntity", spLookEntity, 3);
        addFunction(ctx, sp, "yaw", spYaw, 2);
        addFunction(ctx, sp, "rotationOf", spRotationOf, 1);
        addFunction(ctx, sp, "gameMode", spGameMode, 1);
        addFunction(ctx, sp, "prototype", spPrototype, 1);
        JS_SetPropertyStr(ctx, native, "sp", sp);
    }
}
