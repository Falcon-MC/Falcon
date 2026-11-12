#include "World/Actor/Player/SimulatedPlayerController.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/InventoryHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/InventoryTransactionPacket.h"
#include "Protocol/Packets/PlayerAuthInputPacket.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Item/ElytraItem.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "World/Level/Level.h"

#include <cmath>

namespace {
    const float DEGREES = 57.295776f;
    const float RADIANS = 0.017453292f;
    // moveToLocation stops this close to the target (sub_141035590)
    const float ARRIVAL_DISTANCE = 0.25f;
    const float MINIMUM_STEP = 0.1f;
    // an instant look holds for this many ticks (sub_141039370)
    const int32_t INSTANT_LOOK_TICKS = 20;

    float yawTowards(const Vector3f &from, const Vector3f &to, float current) {
        const float dx = to.x - from.x;
        const float dz = to.z - from.z;
        if (dx == 0.0f && dz == 0.0f)
            return current;
        return std::atan2(dz, dx) * DEGREES - 90.0f;
    }

    // the eyes of an entity: a player's position is its feet here, the game keeps it at eye height
    bool findEntity(ServerNetworkHandler &owner, int64_t uniqueId, Vector3f &eyes) {
        for (auto &entry: owner.getPlayers()) {
            if (entry.second.getUniqueId() == uniqueId && entry.second.isAlive()) {
                const Vector3f &feet = entry.second.getPosition();
                eyes = Vector3f(feet.x, feet.y + MovementPhysics::PLAYER_EYE_HEIGHT, feet.z);
                return true;
            }
        }
        for (auto &entry: owner.getActors()) {
            if (entry.second != nullptr && entry.second->getUniqueId() == uniqueId && entry.second->isAlive()) {
                eyes = entry.second->getPosition();
                return true;
            }
        }
        for (const std::unique_ptr<ItemActor> &item: owner.getItemEntities()) {
            if (item != nullptr && item->getUniqueId() == uniqueId && item->isAlive()) {
                eyes = item->getPosition();
                return true;
            }
        }
        return false;
    }
}

float SimulatedPlayerController::wrapDegrees(float angle) {
    float wrapped = std::fmod(angle + 180.0f, 360.0f);
    if (wrapped < 0.0f)
        wrapped += 360.0f;
    return wrapped - 180.0f;
}

void SimulatedPlayerController::moveInDirection(const Vector3f &direction, bool world) {
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    mDirection = length < 0.0001f ? Vector3f(0.0f, 0.0f, 0.0f)
                                  : Vector3f(direction.x / length, direction.y / length, direction.z / length);
    mWorldDirection = world;
    mMove = Move::Direction;
}

void SimulatedPlayerController::moveToLocation(const Vector3f &target, bool faceTarget, float speed) {
    mTarget = target;
    mSpeed = speed;
    mWorldDirection = faceTarget;
    mMove = Move::ToLocation;
}

void SimulatedPlayerController::stopMoving() {
    mMove = Move::None;
}

// sub_141036BB0: alive, on the ground and past the jump delay
bool SimulatedPlayerController::jump(ServerPlayer &player) {
    const bool onGround = player.hasSimulatedGround() ? player.isSimulatedOnGround() : player.isOnGround();
    if (!player.isAlive() || !onGround)
        return false;
    mJump = true;
    return player.getJumpDelay() == 0;
}

bool SimulatedPlayerController::glide(ServerNetworkHandler &owner, ServerPlayer &player) {
    const bool onGround = player.hasSimulatedGround() ? player.isSimulatedOnGround() : player.isOnGround();
    const LiquidContact liquid = LiquidBlocksFetch::at(owner.getLevelFor(player), player.getPosition());
    if (player.getFlags().get(ActorFlag::Gliding) || !ElytraItem::isWearingElytra(player) || liquid.water || onGround)
        return false;
    mGlideRequest = Request::Start;
    return true;
}

void SimulatedPlayerController::_faceYaw(ServerPlayer &player, const Vector3f &target) {
    Vector3f rotation = player.getRotation();
    rotation.y = wrapDegrees(yawTowards(player.getPosition(), target, rotation.y));
    rotation.z = rotation.y;
    rotation.x = wrapDegrees(rotation.x);
    player.setRotation(rotation);
}

// the look target turns at most 360 degrees a tick: it faces the target at once
void SimulatedPlayerController::_lookAt(ServerPlayer &player, const Vector3f &target) {
    const Vector3f &feet = player.getPosition();
    const Vector3f eyes(feet.x, feet.y + MovementPhysics::PLAYER_EYE_HEIGHT, feet.z);
    const float dx = target.x - eyes.x;
    const float dy = target.y - eyes.y;
    const float dz = target.z - eyes.z;
    Vector3f rotation = player.getRotation();
    rotation.y = wrapDegrees(yawTowards(eyes, target, rotation.y));
    rotation.z = rotation.y;
    rotation.x = wrapDegrees(-std::atan2(dy, std::sqrt(dx * dx + dz * dz)) * DEGREES);
    player.setRotation(rotation);
}

void SimulatedPlayerController::lookAtLocation(ServerPlayer &player, const Vector3f &location, LookDuration duration) {
    _faceYaw(player, location);
    mLook = Look::Location;
    mLookTarget = location;
    mLookTimed = duration == LookDuration::Instant;
    mLookUntilMove = duration == LookDuration::UntilMove;
    mLookTicks = INSTANT_LOOK_TICKS;
}

void SimulatedPlayerController::lookAtEntity(ServerPlayer &player, int64_t uniqueId, const Vector3f &eyes,
                                             LookDuration duration) {
    _faceYaw(player, eyes);
    mLook = Look::Entity;
    mLookEntity = uniqueId;
    mLookTimed = duration == LookDuration::Instant;
    mLookUntilMove = duration == LookDuration::UntilMove;
    mLookTicks = INSTANT_LOOK_TICKS;
}

void SimulatedPlayerController::setBodyRotation(ServerPlayer &player, float yaw) {
    Vector3f rotation = player.getRotation();
    rotation.y = wrapDegrees(yaw);
    rotation.z = rotation.y;
    rotation.x = wrapDegrees(rotation.x);
    player.setRotation(rotation);
    mLook = Look::None;
}

bool SimulatedPlayerController::_actionReady(ServerNetworkHandler &owner) const {
    return !mActed || owner.getCurrentTick() - mLastActionTick >= ACTION_COOLDOWN_TICKS;
}

void SimulatedPlayerController::_markAction(ServerNetworkHandler &owner) {
    mActed = true;
    mLastActionTick = owner.getCurrentTick();
}

bool SimulatedPlayerController::attack(ServerNetworkHandler &owner, ServerPlayer &player, Actor *target) {
    if (!player.isAlive() || !_actionReady(owner))
        return false;
    _markAction(owner);
    // its own id: the swing goes out, no target is found
    return player.attackActor(owner, target == nullptr ? player.getRuntimeId() : target->getRuntimeId(), nullptr,
                              false);
}

bool SimulatedPlayerController::interactWithEntity(ServerNetworkHandler &owner, ServerPlayer &player, Actor &target) {
    if (!player.isAlive())
        return false;
    return InventoryHandler::interactWithEntity(owner, player, target.getRuntimeId(), Vector3f(0.0f, 0.0f, 0.0f));
}

bool SimulatedPlayerController::interactWithBlock(ServerNetworkHandler &owner, ServerPlayer &player,
                                                  const Vector3i &block, int32_t face, const Vector3f &clickPosition) {
    if (!player.isAlive())
        return false;
    ItemUseTransaction transaction;
    transaction.mActionType = 0;
    transaction.mBlockPosition = block;
    transaction.mBlockFace = face;
    transaction.mHotbarSlot = player.getInventory().getSelectedSlot();
    transaction.mItemInHand = player.getInventory().getItemInHand();
    transaction.mPlayerPosition = player.getPosition();
    transaction.mClickPosition = clickPosition;
    transaction.mTriggerType = ItemUseTriggerType::PlayerInput;
    return BlockActionHandler::placeBlock(owner, player, transaction);
}

bool SimulatedPlayerController::useItem(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) {
    if (!player.isAlive() || item.isAir() || !_actionReady(owner))
        return false;
    // the stack is used from the hand and never taken from the inventory
    PlayerInventory &inventory = player.getInventory();
    const ItemStack held = inventory.getItemInHand();
    inventory.setItemInHand(item);
    owner._useHeldItem(player);
    inventory.setItemInHand(held);
    player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, inventory.getSelectedSlot());
    _markAction(owner);
    return true;
}

bool SimulatedPlayerController::useItemInSlot(ServerNetworkHandler &owner, ServerPlayer &player, int32_t slot) {
    PlayerInventory &inventory = player.getInventory();
    if (slot >= 0 && slot < PlayerInventory::HOTBAR_SIZE) {
        inventory.setSelectedSlot(slot);
        player.getInventoryManager().syncSelectedHotbarSlot();
    }
    if (!player.isAlive() || inventory.getItemInHand().isAir() || !_actionReady(owner))
        return false;
    owner._useHeldItem(player);
    _markAction(owner);
    return true;
}

bool SimulatedPlayerController::useItemOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                                               const Vector3i &block, int32_t face, const Vector3f &clickPosition) {
    if (!player.isAlive() || item.isAir() || !_actionReady(owner))
        return false;
    PlayerInventory &inventory = player.getInventory();
    const ItemStack held = inventory.getItemInHand();
    inventory.setItemInHand(item);
    interactWithBlock(owner, player, block, face, clickPosition);
    inventory.setItemInHand(held);
    player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, inventory.getSelectedSlot());
    _markAction(owner);
    return true;
}

bool SimulatedPlayerController::useItemInSlotOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, int32_t slot,
                                                     const Vector3i &block, int32_t face,
                                                     const Vector3f &clickPosition) {
    PlayerInventory &inventory = player.getInventory();
    if (slot >= 0 && slot < PlayerInventory::HOTBAR_SIZE) {
        inventory.setSelectedSlot(slot);
        player.getInventoryManager().syncSelectedHotbarSlot();
    }
    if (!player.isAlive() || inventory.getItemInHand().isAir() || !_actionReady(owner))
        return false;
    interactWithBlock(owner, player, block, face, clickPosition);
    _markAction(owner);
    return true;
}

ItemStack SimulatedPlayerController::stopUsingItem(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (!player.getFlags().get(ActorFlag::UsingItem))
        return ItemStack::air();
    const ItemStack used = player.getInventory().getItemInHand();
    if (!player.isAlive())
        return used;
    // the release a client sends when it lets go of the use button
    InventoryTransactionPacket release;
    release.mTransactionType = InventoryTransactionType::ItemRelease;
    release.mActionType = 0;
    release.mHotbarSlot = player.getInventory().getSelectedSlot();
    release.mItemInHand = used;
    release.mPlayerPosition = player.getPosition();
    InventoryHandler::handleTransaction(owner, player, release);
    return used;
}

bool SimulatedPlayerController::breakBlock(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &block,
                                           int32_t face) {
    if (!player.isAlive())
        return false;
    if (mBreaking) {
        if (mBreakBlock == block && mBreakFace == face)
            return true;
        stopBreakingBlock();
    }
    const BlockState state = owner.getLevelFor(player).getBlockState(block.x, block.y, block.z);
    if (!BlockSupport::isSolid(state) || player.getFlags().get(ActorFlag::UsingItem))
        return false;
    mBreaking = true;
    mBreakBlock = block;
    mBreakFace = face;
    return true;
}

void SimulatedPlayerController::stopBreakingBlock() {
    if (mBreaking)
        mAbortBreak = true;
    mBreaking = false;
}

// the block actions a client sends while it holds the attack button on a block
void SimulatedPlayerController::_breakingActions(ServerNetworkHandler &owner, ServerPlayer &player,
                                                 PlayerAuthInputPacket &packet) {
    if (mAbortBreak) {
        PlayerBlockActionData abort;
        abort.mAction = PlayerActionType::AbortBreak;
        abort.mBlockPosition = player.isBreakingBlock() ? player.getBreakingBlockPosition() : mBreakBlock;
        packet.mPlayerActions.push_back(abort);
        mAbortBreak = false;
    }
    if (!mBreaking)
        return;
    const BlockState state = owner.getLevelFor(player).getBlockState(mBreakBlock.x, mBreakBlock.y, mBreakBlock.z);
    if (state.mName == "minecraft:air") {
        mBreaking = false;
        return;
    }
    PlayerBlockActionData action;
    action.mBlockPosition = mBreakBlock;
    action.mFace = mBreakFace;
    if (!player.isBreakingBlock() || player.getBreakingBlockPosition() != mBreakBlock) {
        action.mAction = PlayerActionType::StartBreak;
        packet.mPlayerActions.push_back(action);
        if (!player.isCreative())
            return;
        action.mAction = PlayerActionType::DimensionChangeRequestOrCreativeDestroyBlock;
        packet.mPlayerActions.push_back(action);
        return;
    }
    if (player.getBreakProgress() < 1.0)
        return;
    action.mAction = PlayerActionType::BlockPredictDestroy;
    packet.mPlayerActions.push_back(action);
}

// sub_141035590: the order becomes a move vector relative to the yaw, then the look target turns the head
void SimulatedPlayerController::buildInput(ServerNetworkHandler &owner, ServerPlayer &player,
                                           PlayerAuthInputPacket &packet) {
    float sideways = 0.0f;
    float forward = 0.0f;
    const float yaw = player.getRotation().y * RADIANS;
    if (mMove == Move::Direction) {
        if (mWorldDirection) {
            sideways = mDirection.x * std::cos(yaw) + mDirection.z * std::sin(yaw);
            forward = mDirection.z * std::cos(yaw) - mDirection.x * std::sin(yaw);
        } else {
            sideways = mDirection.x;
            forward = mDirection.z;
        }
    } else if (mMove == Move::ToLocation) {
        const Vector3f &feet = player.getPosition();
        const float dx = mTarget.x - feet.x;
        const float dy = mTarget.y - feet.y;
        const float dz = mTarget.z - feet.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (distance < ARRIVAL_DISTANCE) {
            mMove = Move::None;
        } else {
            if (mWorldDirection) {
                _faceYaw(player, mTarget);
                mLook = Look::Location;
                mLookTarget = mTarget;
                mLookTimed = false;
                mLookUntilMove = true;
            }
            const float step = std::fmin(mSpeed, std::fmax(distance, MINIMUM_STEP));
            const float horizontal = std::sqrt(dx * dx + dz * dz);
            const float scale = horizontal == 0.0f ? 0.0f : step / horizontal;
            const float turned = player.getRotation().y * RADIANS;
            sideways = (dx * std::cos(turned) + dz * std::sin(turned)) * scale;
            forward = (dz * std::cos(turned) - dx * std::sin(turned)) * scale;
        }
    }

    if (mLook != Look::None) {
        Vector3f target = mLookTarget;
        bool keep = mLook == Look::Location || findEntity(owner, mLookEntity, target);
        if (keep && !mLookTimed && mLookUntilMove && (sideways != 0.0f || forward != 0.0f))
            keep = false;
        if (keep)
            _lookAt(player, target);
        if (!keep || (mLookTimed && mLookTicks-- < 0))
            mLook = Look::None;
    }

    const Vector3f &feet = player.getPosition();
    packet.mPosition = Vector3f(feet.x, feet.y + MovementPhysics::PLAYER_BASE_OFFSET, feet.z);
    packet.mRotation = player.getRotation();
    packet.mCameraOrientation = Vector3f(0.0f, 0.0f, 0.0f);
    packet.mMotionX = sideways;
    packet.mMotionY = forward;
    packet.mAnalogMoveVectorX = sideways;
    packet.mAnalogMoveVectorY = forward;
    packet.mRawMoveVectorX = sideways;
    packet.mRawMoveVectorY = forward;
    packet.mInputMode = PlayerInputMode::Mouse;
    packet.mDelta = player.getMotion();
    packet.mTick = owner.getCurrentTick();

    if (mJump) {
        packet.mInputData.push_back((int32_t) PlayerAuthInputData::StartJumping);
        packet.mInputData.push_back((int32_t) PlayerAuthInputData::Jumping);
        mJump = false;
    }
    if (mSwimRequest != Request::None)
        packet.mInputData.push_back((int32_t) (mSwimRequest == Request::Start ? PlayerAuthInputData::StartSwimming
                                                                              : PlayerAuthInputData::StopSwimming));
    if (mGlideRequest != Request::None)
        packet.mInputData.push_back((int32_t) (mGlideRequest == Request::Start ? PlayerAuthInputData::StartGliding
                                                                               : PlayerAuthInputData::StopGliding));
    mSwimRequest = Request::None;
    mGlideRequest = Request::None;
    _breakingActions(owner, player, packet);
    const bool sprinting = player.getFlags().get(ActorFlag::Sprinting);
    if (mSprinting) {
        packet.mInputData.push_back((int32_t) PlayerAuthInputData::Sprinting);
        if (!sprinting)
            packet.mInputData.push_back((int32_t) PlayerAuthInputData::StartSprinting);
    } else if (sprinting) {
        packet.mInputData.push_back((int32_t) PlayerAuthInputData::StopSprinting);
    }
}
