#include "Scripting/Binding/ScriptSimulatedPlayerBinding.h"

#include "Network/ServerNetworkHandler.h"
#include "Network/Handler/LoginHandler.h"
#include "Protocol/Packets/PlayerSkinPacket.h"
#include "Protocol/Packets/TextPacket.h"
#include "Scripting/Binding/ScriptApi.h"
#include "Scripting/Binding/ScriptNatives.h"
#include "Scripting/Binding/ScriptRayQueries.h"
#include "Scripting/Binding/ScriptRaycast.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "World/Actor/Player/ServerPlayer.h"

#include <algorithm>
#include <cstring>
#include <vector>

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
        return JS_NewBool(ctx, player != nullptr && player->getSimulatedController()->jump(api->host(), *player));
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

    // (player, swimming)
    JSValue spSwim(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 2 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr)
            return JS_FALSE;
        player->getSimulatedController()->setSwimming(JS_ToBool(ctx, argv[1]) == 1);
        return JS_TRUE;
    }

    JSValue spGlide(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 1 ? simulatedOf(*api, argv[0]) : nullptr;
        return JS_NewBool(ctx, player != nullptr && player->getSimulatedController()->glide(api->host(), *player));
    }

    JSValue spStopGliding(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 1 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr)
            return JS_FALSE;
        player->getSimulatedController()->stopGliding();
        return JS_TRUE;
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

    // an ItemStack value, false for anything else
    bool itemOf(ScriptApi &api, JSValueConst value, ItemStack &item) {
        const ScriptItemRef *ref = api.native<ScriptItemRef>(value);
        return ref != nullptr && ref->mComponentId.empty() && api.toItemStack(*ref->mItem, item);
    }

    Vector3i blockOf(JSContext *ctx, JSValueConst *argv) {
        int32_t x = 0, y = 0, z = 0;
        JS_ToInt32(ctx, &x, argv[0]);
        JS_ToInt32(ctx, &y, argv[1]);
        JS_ToInt32(ctx, &z, argv[2]);
        return Vector3i(x, y, z);
    }

    int32_t intOf(JSContext *ctx, JSValueConst value) {
        int32_t result = 0;
        JS_ToInt32(ctx, &result, value);
        return result;
    }

    Vector3f eyesOf(const ServerPlayer &player) {
        const Vector3f &feet = player.getPosition();
        return Vector3f(feet.x, feet.y + MovementPhysics::PLAYER_EYE_HEIGHT, feet.z);
    }

    // the first entity on the player's view ray, blocks stopping it
    Actor *entityInView(JSContext *ctx, ScriptApi &api, ServerPlayer &player, float distance) {
        JSValue options = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, options, "maxDistance", JS_NewFloat64(ctx, distance));
        JSValue hits = ScriptRayQueries::entitiesFromRay(ctx, api.host().getLevelFor(player), eyesOf(player),
                                                         player.getLookDirection(), options, &player);
        JS_FreeValue(ctx, options);
        JSValue first = JS_GetPropertyUint32(ctx, hits, 0);
        Actor *found = nullptr;
        if (JS_IsObject(first)) {
            JSValue entity = JS_GetPropertyStr(ctx, first, "entity");
            found = api.resolveEntity(entity);
            JS_FreeValue(ctx, entity);
        }
        JS_FreeValue(ctx, first);
        JS_FreeValue(ctx, hits);
        return found;
    }

    int32_t faceIndex(const char *face) {
        static const char *const FACES[] = {"Down", "Up", "North", "South", "West", "East"};
        for (int32_t index = 0; index < 6; ++index) {
            if (std::strcmp(FACES[index], face) == 0)
                return index;
        }
        return 1;
    }

    // sub_141037E70: creative reaches 7 blocks, any other mode 3
    JSValue spAttack(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 1 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr)
            return JS_FALSE;
        Actor *target = entityInView(ctx, *api, *player, player->isCreative() ? 7.0f : 3.0f);
        return JS_NewBool(ctx, player->getSimulatedController()->attack(api->host(), *player, target));
    }

    // (player, entity): undefined when the entity is gone
    JSValue spAttackEntity(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 2 ? simulatedOf(*api, argv[0]) : nullptr;
        Actor *target = argc >= 2 ? api->resolveEntity(argv[1]) : nullptr;
        if (player == nullptr || target == nullptr)
            return JS_UNDEFINED;
        return JS_NewBool(ctx, player->getSimulatedController()->attack(api->host(), *player, target));
    }

    // (player, entity): undefined when the entity is gone
    JSValue spInteractEntity(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 2 ? simulatedOf(*api, argv[0]) : nullptr;
        Actor *target = argc >= 2 ? api->resolveEntity(argv[1]) : nullptr;
        if (player == nullptr || target == nullptr)
            return JS_UNDEFINED;
        return JS_NewBool(ctx, player->getSimulatedController()->interactWithEntity(api->host(), *player, *target));
    }

    // sub_141038A50: a 5 block ray, the entity in front of the block wins
    JSValue spInteract(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 1 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr || !player->isAlive())
            return JS_FALSE;
        const float distance = 5.0f;
        SimulatedPlayerController &controller = *player->getSimulatedController();
        if (Actor *target = entityInView(ctx, *api, *player, distance))
            return JS_NewBool(ctx, controller.interactWithEntity(api->host(), *player, *target));
        ScriptRaycast::BlockOptions ray;
        ray.mMaxDistance = distance;
        ScriptRaycast::BlockHit hit;
        if (!ScriptRaycast::castBlocks(api->host().getLevelFor(*player), eyesOf(*player), player->getLookDirection(),
                                       ray, hit))
            return JS_FALSE;
        return JS_NewBool(ctx, controller.interactWithBlock(api->host(), *player, hit.mBlock, faceIndex(hit.mFace),
                                                            hit.mFaceLocation));
    }

    // (player, x, y, z, face)
    JSValue spUseBlock(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 5 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr)
            return JS_FALSE;
        return JS_NewBool(ctx, player->getSimulatedController()->useBlock(api->host(), *player, blockOf(ctx, argv + 1),
                                                                          intOf(ctx, argv[4])));
    }

    // (player, item): null for no ItemStack
    JSValue spUseItem(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 2 ? simulatedOf(*api, argv[0]) : nullptr;
        ItemStack item;
        if (player == nullptr || !itemOf(*api, argv[1], item))
            return JS_NULL;
        return JS_NewBool(ctx, player->getSimulatedController()->useItem(api->host(), *player, item));
    }

    JSValue spUseSlot(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 2 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr)
            return JS_FALSE;
        return JS_NewBool(ctx,
                          player->getSimulatedController()->useItemInSlot(api->host(), *player, intOf(ctx, argv[1])));
    }

    // (player, item, x, y, z, face, click x, y, z): null for no ItemStack
    JSValue spUseItemOnBlock(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 9 ? simulatedOf(*api, argv[0]) : nullptr;
        ItemStack item;
        if (player == nullptr || !itemOf(*api, argv[1], item))
            return JS_NULL;
        return JS_NewBool(ctx, player->getSimulatedController()->useItemOnBlock(
                                       api->host(), *player, item, blockOf(ctx, argv + 2), intOf(ctx, argv[5]),
                                       Vector3f(floatOf(ctx, argv[6]), floatOf(ctx, argv[7]), floatOf(ctx, argv[8]))));
    }

    // (player, slot, x, y, z, face, click x, y, z)
    JSValue spUseSlotOnBlock(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 9 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr)
            return JS_FALSE;
        return JS_NewBool(ctx, player->getSimulatedController()->useItemInSlotOnBlock(
                                       api->host(), *player, intOf(ctx, argv[1]), blockOf(ctx, argv + 2),
                                       intOf(ctx, argv[5]),
                                       Vector3f(floatOf(ctx, argv[6]), floatOf(ctx, argv[7]), floatOf(ctx, argv[8]))));
    }

    JSValue spStopUsing(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 1 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr)
            return JS_UNDEFINED;
        const ItemStack used = player->getSimulatedController()->stopUsingItem(api->host(), *player);
        return used.isAir() ? JS_UNDEFINED : api->makeItem(used);
    }

    // (player, x, y, z, face)
    JSValue spBreak(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 5 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr)
            return JS_FALSE;
        return JS_NewBool(ctx, player->getSimulatedController()->breakBlock(
                                       api->host(), *player, blockOf(ctx, argv + 1), intOf(ctx, argv[4])));
    }

    JSValue spStopBreaking(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 1 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player != nullptr)
            player->getSimulatedController()->stopBreakingBlock();
        return JS_UNDEFINED;
    }

    // sub_141039C20: a chat TextPacket handed to the server as if the client sent it
    JSValue spChat(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 2 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr)
            return JS_UNDEFINED;
        TextPacket packet;
        packet.mType = TextPacket::Type::Chat;
        packet.mSourceName = player->getName();
        packet.mMessage = toString(ctx, argv[1]);
        packet.mXuid = player->getXuid();
        packet.handle(player->getNetworkIdentifier(), api->host());
        return JS_UNDEFINED;
    }

    // sub_14DCFDE50: the whole selected stack, the slot emptied
    JSValue spDrop(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 1 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr || player->getInventory().getItemInHand().isAir())
            return JS_FALSE;
        PlayerInventory &inventory = player->getInventory();
        const ItemStack dropped = inventory.getItemInHand();
        inventory.setItemInHand(ItemStack::air());
        player->getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, inventory.getSelectedSlot());
        api->host()._throwItem(*player, dropped);
        return JS_TRUE;
    }

    // (player, flying): the Flying ability, whatever the game mode allows (sub_14DCFE250)
    JSValue spFly(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 2 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr)
            return JS_UNDEFINED;
        player->setFlying(JS_ToBool(ctx, argv[1]) == 1);
        api->host()._sendAbilities(*player);
        return JS_UNDEFINED;
    }

    // (player, item, select): sub_141038940, Player::add then the pick block selection
    JSValue spGive(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 3 ? simulatedOf(*api, argv[0]) : nullptr;
        ItemStack item;
        if (player == nullptr || !itemOf(*api, argv[1], item))
            return JS_NULL;
        std::vector<int> touched;
        const bool added = player->getInventory().addItem(item, touched);
        for (const int slot: touched)
            player->getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
        if (added && JS_ToBool(ctx, argv[2]) == 1)
            api->host().pickItem(*player, item, player->getInventory().getSelectedSlot());
        return JS_NewBool(ctx, added);
    }

    // (player, item, slot, select): sub_14DCFDCC0, a hotbar slot only gets selected
    JSValue spSetItem(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 4 ? simulatedOf(*api, argv[0]) : nullptr;
        ItemStack item;
        if (player == nullptr || !itemOf(*api, argv[1], item))
            return JS_NULL;
        const int32_t slot = intOf(ctx, argv[2]);
        if (slot < 0 || slot >= PlayerInventory::CONTAINER_SIZE)
            return JS_FALSE;
        player->getInventory().setItem(slot, item);
        player->getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
        if (slot < PlayerInventory::HOTBAR_SIZE && JS_ToBool(ctx, argv[3]) == 1) {
            player->getInventory().setSelectedSlot(slot);
            player->getInventoryManager().syncSelectedHotbarSlot();
        }
        return JS_TRUE;
    }

    // sub_14DCFDFA0: only a dead player respawns
    JSValue spRespawn(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 1 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr || !player->isDead())
            return JS_FALSE;
        api->host().respawnSimulatedPlayer(*player);
        return JS_TRUE;
    }

    // (player, slot)
    JSValue spStartBuild(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 2 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player != nullptr)
            player->getSimulatedController()->startBuild(api->host(), *player, intOf(ctx, argv[1]));
        return JS_UNDEFINED;
    }

    JSValue spStopBuild(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 1 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player != nullptr)
            player->getSimulatedController()->stopBuild();
        return JS_UNDEFINED;
    }

    // Player vtable +0x6E0 (sub_1402C7640): the open container closes
    JSValue spStopInteracting(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 1 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr)
            return JS_UNDEFINED;
        InventoryManager &manager = player->getInventoryManager();
        if (manager.isContainerOpen() || manager.isFurnaceOpen() || manager.isCraftingTableOpen()
            || manager.isAnvilOpen())
            manager.onClientRemoveWindow(manager.getCurrentWindowId());
        return JS_UNDEFINED;
    }

    // mce::Color to ARGB: each channel clamped to [0, 1], times 255, truncated (sub_141996440)
    int32_t argbOf(float red, float green, float blue, float alpha) {
        const auto channel = [](float value) { return (uint32_t) (std::min(1.0f, std::max(0.0f, value)) * 255.0f); };
        return (int32_t) ((channel(alpha) << 24) | (channel(red) << 16) | (channel(green) << 8) | channel(blue));
    }

    // (player, arm size 0 slim / 1 wide, { red, green, blue } or null, [{ id, type, packId, productId,
    // isDefaultPiece }] or null): sub_14DCFE3B0, the tints dropped, wide arms and white skin by default
    JSValue spSetSkin(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 4 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr)
            return JS_FALSE;
        SerializedSkin skin = player->getSkin();
        skin.mArmsWide = intOf(ctx, argv[1]) != 0;
        float red = 1.0f, green = 1.0f, blue = 1.0f;
        if (JS_IsObject(argv[2])) {
            JSValue value = JS_GetPropertyStr(ctx, argv[2], "red");
            red = floatOf(ctx, value);
            JS_FreeValue(ctx, value);
            value = JS_GetPropertyStr(ctx, argv[2], "green");
            green = floatOf(ctx, value);
            JS_FreeValue(ctx, value);
            value = JS_GetPropertyStr(ctx, argv[2], "blue");
            blue = floatOf(ctx, value);
            JS_FreeValue(ctx, value);
        }
        skin.mColorArgb = argbOf(red, green, blue, 1.0f);
        skin.mPersonaPieces.clear();
        skin.mTintColors.clear();
        if (JS_IsObject(argv[3])) {
            JSValue length = JS_GetPropertyStr(ctx, argv[3], "length");
            const int32_t count = intOf(ctx, length);
            JS_FreeValue(ctx, length);
            for (int32_t index = 0; index < count; ++index) {
                JSValue entry = JS_GetPropertyUint32(ctx, argv[3], (uint32_t) index);
                PersonaPieceData piece;
                JSValue field = JS_GetPropertyStr(ctx, entry, "id");
                piece.mId = toString(ctx, field);
                JS_FreeValue(ctx, field);
                field = JS_GetPropertyStr(ctx, entry, "type");
                piece.mPieceType = intOf(ctx, field);
                JS_FreeValue(ctx, field);
                field = JS_GetPropertyStr(ctx, entry, "packId");
                piece.mPackId = Uuid::fromString(toString(ctx, field));
                JS_FreeValue(ctx, field);
                field = JS_GetPropertyStr(ctx, entry, "productId");
                piece.mProductId = toString(ctx, field);
                JS_FreeValue(ctx, field);
                field = JS_GetPropertyStr(ctx, entry, "isDefaultPiece");
                piece.mIsDefault = JS_ToBool(ctx, field) == 1;
                JS_FreeValue(ctx, field);
                JS_FreeValue(ctx, entry);
                skin.mPersonaPieces.push_back(piece);
            }
        }
        player->setSkin(skin);
        // a PlayerSkinPacket to everyone, its names "new_id" and "old_id" (sub_14404E3F0)
        PlayerSkinPacket packet;
        packet.mUuid = LoginHandler::playerListUuid(*player);
        packet.mSkin = player->getSkin();
        packet.mNewSkinName = "new_id";
        packet.mOldSkinName = "old_id";
        packet.mTrustedSkin = packet.mSkin.mTrusted;
        ServerNetworkHandler &host = api->host();
        for (auto &entry: host.getPlayers()) {
            if (entry.second.isSpawned() && !entry.second.isSimulated())
                host.getNetworkHandler().send(entry.second.getNetworkIdentifier(), packet,
                                              host.getCodecContext(entry.second.getNetworkIdentifier()));
        }
        return JS_TRUE;
    }

    // (player): { armSize, color: { red, green, blue }, pieces: [...] }, undefined for anything but a player
    JSValue spSkinOf(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 1 ? api->resolvePlayer(argv[0]) : nullptr;
        if (player == nullptr)
            return JS_UNDEFINED;
        const SerializedSkin &skin = player->getSkin();
        JSValue result = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, result, "armSize", JS_NewInt32(ctx, skin.mArmsWide ? 1 : 0));
        const uint32_t argb = (uint32_t) skin.mColorArgb;
        JSValue color = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, color, "red", JS_NewFloat64(ctx, (float) ((argb >> 16) & 0xFF) / 255.0f));
        JS_SetPropertyStr(ctx, color, "green", JS_NewFloat64(ctx, (float) ((argb >> 8) & 0xFF) / 255.0f));
        JS_SetPropertyStr(ctx, color, "blue", JS_NewFloat64(ctx, (float) (argb & 0xFF) / 255.0f));
        JS_SetPropertyStr(ctx, result, "color", color);
        JSValue pieces = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const PersonaPieceData &piece: skin.mPersonaPieces) {
            JSValue entry = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, entry, "id", JS_NewString(ctx, piece.mId.c_str()));
            JS_SetPropertyStr(ctx, entry, "type", JS_NewInt32(ctx, piece.mPieceType));
            JS_SetPropertyStr(ctx, entry, "packId", JS_NewString(ctx, piece.mPackId.toString().c_str()));
            JS_SetPropertyStr(ctx, entry, "productId", JS_NewString(ctx, piece.mProductId.c_str()));
            JS_SetPropertyStr(ctx, entry, "isDefaultPiece", JS_NewBool(ctx, piece.mIsDefault));
            JS_SetPropertyUint32(ctx, pieces, index++, entry);
        }
        JS_SetPropertyStr(ctx, result, "pieces", pieces);
        return result;
    }

    JSValue navigationResult(JSContext *ctx, bool fullPath, const std::vector<Vector3i> &path) {
        JSValue result = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, result, "full", JS_NewBool(ctx, fullPath));
        JSValue points = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const Vector3i &point: path) {
            JSValue entry = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, entry, "x", JS_NewInt32(ctx, point.x));
            JS_SetPropertyStr(ctx, entry, "y", JS_NewInt32(ctx, point.y));
            JS_SetPropertyStr(ctx, entry, "z", JS_NewInt32(ctx, point.z));
            JS_SetPropertyUint32(ctx, points, index++, entry);
        }
        JS_SetPropertyStr(ctx, result, "path", points);
        return result;
    }

    // (player, x, y, z, speed): { full, path }
    JSValue spNavigate(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 5 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr)
            return JS_UNDEFINED;
        std::vector<Vector3i> path;
        bool fullPath = false;
        player->getSimulatedController()->navigateToLocation(
                api->host(), *player, Vector3f(floatOf(ctx, argv[1]), floatOf(ctx, argv[2]), floatOf(ctx, argv[3])),
                floatOf(ctx, argv[4]), path, fullPath);
        return navigationResult(ctx, fullPath, path);
    }

    // (player, entity, speed): undefined when the entity is gone
    JSValue spNavigateEntity(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 3 ? simulatedOf(*api, argv[0]) : nullptr;
        Actor *target = argc >= 3 ? api->resolveEntity(argv[1]) : nullptr;
        if (player == nullptr || target == nullptr)
            return JS_UNDEFINED;
        std::vector<Vector3i> path;
        bool fullPath = false;
        player->getSimulatedController()->navigateToEntity(api->host(), *player, target->getUniqueId(),
                                                           target->getPosition(), floatOf(ctx, argv[2]), path,
                                                           fullPath);
        return navigationResult(ctx, fullPath, path);
    }

    // (player, [{ x, y, z }], speed)
    JSValue spNavigateRoute(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        ServerPlayer *player = argc >= 3 ? simulatedOf(*api, argv[0]) : nullptr;
        if (player == nullptr)
            return JS_UNDEFINED;
        std::vector<Vector3f> route;
        JSValue length = JS_GetPropertyStr(ctx, argv[1], "length");
        const int32_t count = intOf(ctx, length);
        JS_FreeValue(ctx, length);
        for (int32_t index = 0; index < count; ++index) {
            JSValue entry = JS_GetPropertyUint32(ctx, argv[1], (uint32_t) index);
            JSValue x = JS_GetPropertyStr(ctx, entry, "x");
            JSValue y = JS_GetPropertyStr(ctx, entry, "y");
            JSValue z = JS_GetPropertyStr(ctx, entry, "z");
            route.emplace_back(floatOf(ctx, x), floatOf(ctx, y), floatOf(ctx, z));
            JS_FreeValue(ctx, x);
            JS_FreeValue(ctx, y);
            JS_FreeValue(ctx, z);
            JS_FreeValue(ctx, entry);
        }
        player->getSimulatedController()->navigateToLocations(route, floatOf(ctx, argv[2]));
        return JS_UNDEFINED;
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
        addFunction(ctx, sp, "swim", spSwim, 2);
        addFunction(ctx, sp, "glide", spGlide, 1);
        addFunction(ctx, sp, "stopGliding", spStopGliding, 1);
        addFunction(ctx, sp, "look", spLook, 5);
        addFunction(ctx, sp, "lookEntity", spLookEntity, 3);
        addFunction(ctx, sp, "yaw", spYaw, 2);
        addFunction(ctx, sp, "rotationOf", spRotationOf, 1);
        addFunction(ctx, sp, "attack", spAttack, 1);
        addFunction(ctx, sp, "attackEntity", spAttackEntity, 2);
        addFunction(ctx, sp, "interact", spInteract, 1);
        addFunction(ctx, sp, "interactEntity", spInteractEntity, 2);
        addFunction(ctx, sp, "useBlock", spUseBlock, 5);
        addFunction(ctx, sp, "useItem", spUseItem, 2);
        addFunction(ctx, sp, "useSlot", spUseSlot, 2);
        addFunction(ctx, sp, "useItemOnBlock", spUseItemOnBlock, 9);
        addFunction(ctx, sp, "useSlotOnBlock", spUseSlotOnBlock, 9);
        addFunction(ctx, sp, "stopUsing", spStopUsing, 1);
        addFunction(ctx, sp, "breakBlock", spBreak, 5);
        addFunction(ctx, sp, "stopBreaking", spStopBreaking, 1);
        addFunction(ctx, sp, "chat", spChat, 2);
        addFunction(ctx, sp, "drop", spDrop, 1);
        addFunction(ctx, sp, "fly", spFly, 2);
        addFunction(ctx, sp, "give", spGive, 3);
        addFunction(ctx, sp, "setItem", spSetItem, 4);
        addFunction(ctx, sp, "respawn", spRespawn, 1);
        addFunction(ctx, sp, "startBuild", spStartBuild, 2);
        addFunction(ctx, sp, "stopBuild", spStopBuild, 1);
        addFunction(ctx, sp, "stopInteracting", spStopInteracting, 1);
        addFunction(ctx, sp, "setSkin", spSetSkin, 4);
        addFunction(ctx, sp, "skinOf", spSkinOf, 1);
        addFunction(ctx, sp, "navigate", spNavigate, 5);
        addFunction(ctx, sp, "navigateEntity", spNavigateEntity, 3);
        addFunction(ctx, sp, "navigateRoute", spNavigateRoute, 3);
        addFunction(ctx, sp, "gameMode", spGameMode, 1);
        addFunction(ctx, sp, "prototype", spPrototype, 1);
        JS_SetPropertyStr(ctx, native, "sp", sp);
    }
}
