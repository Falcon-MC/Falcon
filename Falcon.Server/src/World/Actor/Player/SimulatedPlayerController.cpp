#include "World/Actor/Player/SimulatedPlayerController.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/InventoryHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/InventoryTransactionPacket.h"
#include "Protocol/Packets/PlayerAuthInputPacket.h"
#include "Scripting/Binding/ScriptRaycast.h"
#include "World/Actor/AI/Navigation/Path.h"
#include "World/Actor/AI/Navigation/PathFinder.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Item/ElytraItem.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "Scripting/ScriptEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
    const float DEGREES = 57.295776f;
    const float RADIANS = 0.017453292f;
    // moveToLocation stops this close to the target (sub_141035590)
    const float ARRIVAL_DISTANCE = 0.25f;
    const float MINIMUM_STEP = 0.1f;
    // an instant look holds for this many ticks (sub_141039370)
    const int32_t INSTANT_LOOK_TICKS = 20;
    // startBuild picks the block in view this far (sub_141031E80)
    const float BUILD_REACH = 10.0f;
    // GameMode::continueBuildBlock (sub_14376BB80): the repeat delay in milliseconds
    const float BUILD_DELAY_MS = 200.0f;
    const float SNEAK_BUILD_DELAY_MS = 300.0f;
    const float MOVING_BUILD_DELAY_MS = 180.0f;
    const float MOVING_BUILD_DISTANCE_MS = 900.0f;
    const float MINIMUM_BUILD_DELAY_MS = 100.0f;
    const int64_t BUILD_CATCH_UP_MS = 180;
    // PathNavigation's stuck rule: the walk stops after this many ticks without moving
    const int32_t NAVIGATION_STUCK_TICKS = 40;
    const float NAVIGATION_STUCK_DISTANCE_SQUARED = 1.0e-4f;
    // a navigated entity is followed to within a block (SimulatedPlayer.navigateToEntity)
    const float FOLLOW_DISTANCE = 1.0f;

    int32_t faceOf(const char *face) {
        static const char *const FACES[] = {"Down", "Up", "North", "South", "West", "East"};
        for (int32_t index = 0; index < 6; ++index) {
            if (std::strcmp(FACES[index], face) == 0)
                return index;
        }
        return 1;
    }

    const int32_t FACE_Y[6] = {-1, 1, 0, 0, 0, 0};

    Vector3f eyesOf(const ServerPlayer &player) {
        const Vector3f &feet = player.getPosition();
        return Vector3f(feet.x, feet.y + MovementPhysics::PLAYER_EYE_HEIGHT, feet.z);
    }

    float yawTowards(const Vector3f &from, const Vector3f &to, float current) {
        const float dx = to.x - from.x;
        const float dz = to.z - from.z;
        if (dx == 0.0f && dz == 0.0f)
            return current;
        return std::atan2(dz, dx) * DEGREES - 90.0f;
    }

    // the eyes of an entity: a player's position is its feet here, the game keeps it at eye height
    bool findEntity(ServerNetworkHandler &owner, int64_t uniqueId, Vector3f &eyes, bool atEyes = true) {
        for (auto &entry: owner.getPlayers()) {
            if (entry.second.getUniqueId() == uniqueId && entry.second.isAlive()) {
                const Vector3f &feet = entry.second.getPosition();
                eyes = Vector3f(feet.x, feet.y + (atEyes ? MovementPhysics::PLAYER_EYE_HEIGHT : 0.0f), feet.z);
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
    mFollowEntity = 0;
    mRoute.clear();
    mWaypoints.clear();
}

// sub_141036BB0: alive, on the ground and past the jump delay
bool SimulatedPlayerController::jump(ServerNetworkHandler &owner, ServerPlayer &player) {
    const bool onGround = player.hasSimulatedGround() ? player.isSimulatedOnGround() : player.isOnGround();
    if (!player.isAlive() || !onGround)
        return false;
    mJump = true;
    // Mob::jumpFromGround runs at once and starts the 10 tick delay: a second call this tick finds it running
    if (player.getJumpDelay() != 0 || mJumpTick == owner.getCurrentTick())
        return false;
    mJumpTick = owner.getCurrentTick();
    return true;
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

// sub_14DD00600: Block::use (sub_143168110) on the block, no item and no click position
bool SimulatedPlayerController::useBlock(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &block,
                                         int32_t face) {
    if (!player.isAlive())
        return false;
    const BlockState state = owner.getLevelFor(player).getBlockState(block.x, block.y, block.z);
    owner.getScriptEngine().onCustomBlockUse(player, state.mName, block, face, Vector3f(0.0f, 0.0f, 0.0f));
    const Block *type = VanillaBlocks::fromIdentifier(state.mName);
    return type != nullptr && type->onInteract(owner, player, block, state);
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
    // BlockLegacy vtable +528: false only for air, fire and soul fire (26.60)
    const BlockState state = owner.getLevelFor(player).getBlockState(block.x, block.y, block.z);
    if (state.mName == "minecraft:air" || state.mName == "minecraft:fire" || state.mName == "minecraft:soul_fire"
        || player.getFlags().get(ActorFlag::UsingItem))
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
    float vertical = 0.0f;
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
        if (std::sqrt(dx * dx + dy * dy + dz * dz) < ARRIVAL_DISTANCE)
            mMove = Move::None;
        else
            _stepTowards(player, mTarget, mSpeed, mWorldDirection, sideways, forward, vertical);
    } else if (mMove == Move::Navigate) {
        _tickNavigation(owner, player, sideways, forward, vertical);
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
    // a flying player's move vector has its height: played as the flight's up and down inputs
    if (vertical > 0.0f)
        packet.mInputData.push_back((int32_t) PlayerAuthInputData::WantUp);
    else if (vertical < 0.0f)
        packet.mInputData.push_back((int32_t) PlayerAuthInputData::WantDown);
    if (mSwimRequest != Request::None)
        packet.mInputData.push_back((int32_t) (mSwimRequest == Request::Start ? PlayerAuthInputData::StartSwimming
                                                                              : PlayerAuthInputData::StopSwimming));
    if (mGlideRequest != Request::None)
        packet.mInputData.push_back((int32_t) (mGlideRequest == Request::Start ? PlayerAuthInputData::StartGliding
                                                                               : PlayerAuthInputData::StopGliding));
    mSwimRequest = Request::None;
    mGlideRequest = Request::None;
    _breakingActions(owner, player, packet);
    if (mBuilding) {
        if (player.isAlive())
            _continueBuild(owner, player);
        else
            stopBuild();
    }
    const bool sprinting = player.getFlags().get(ActorFlag::Sprinting);
    if (mSprinting) {
        packet.mInputData.push_back((int32_t) PlayerAuthInputData::Sprinting);
        if (!sprinting)
            packet.mInputData.push_back((int32_t) PlayerAuthInputData::StartSprinting);
    } else if (sprinting) {
        packet.mInputData.push_back((int32_t) PlayerAuthInputData::StopSprinting);
    }
}

// sub_141035590: towards the point at most `speed` a tick; only a flying player keeps the height difference
void SimulatedPlayerController::_stepTowards(ServerPlayer &player, const Vector3f &target, float speed, bool faceTarget,
                                             float &sideways, float &forward, float &vertical) {
    const Vector3f &feet = player.getPosition();
    const float dx = target.x - feet.x;
    const float dz = target.z - feet.z;
    const float distance = std::sqrt(dx * dx + (target.y - feet.y) * (target.y - feet.y) + dz * dz);
    if (faceTarget) {
        _faceYaw(player, target);
        mLook = Look::Location;
        mLookTarget = target;
        mLookTimed = false;
        mLookUntilMove = true;
    }
    const float step = std::fmin(speed, std::fmax(distance, MINIMUM_STEP));
    // sub_1402BF7D0: the flying ability
    const float dy = player.isFlying() ? target.y - feet.y : 0.0f;
    const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (length == 0.0f)
        return;
    const float scale = step / length;
    const float turned = player.getRotation().y * RADIANS;
    sideways = (dx * std::cos(turned) + dz * std::sin(turned)) * scale;
    forward = (dz * std::cos(turned) - dx * std::sin(turned)) * scale;
    vertical = dy * scale;
}

bool SimulatedPlayerController::_buildOn(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &block,
                                         int32_t face, const Vector3f &clickPosition, bool continued) {
    const auto now = std::chrono::steady_clock::now();
    if (!continued || !mHasBuilt) {
        if (!interactWithBlock(owner, player, block, face, clickPosition))
            return false;
        mHasBuilt = true;
        mLastBuild = now;
        return true;
    }
    // GameMode::continueBuildBlock: sneaking or standing repeats at a fixed pace, moving faster with the speed
    const Vector3f &motion = player.getMotion();
    const float speed = std::sqrt(motion.x * motion.x + motion.y * motion.y + motion.z * motion.z) * 20.0f;
    const bool sneaking = player.getFlags().get(ActorFlag::Sneaking);
    float delay = sneaking ? SNEAK_BUILD_DELAY_MS : BUILD_DELAY_MS;
    if (!sneaking && speed > 0.0f)
        delay = std::fmin(MOVING_BUILD_DISTANCE_MS / speed, MOVING_BUILD_DELAY_MS);
    delay = std::fmax(MINIMUM_BUILD_DELAY_MS, delay);
    const auto due = mLastBuild + std::chrono::milliseconds((int64_t) delay);
    if (due >= now || !interactWithBlock(owner, player, block, face, clickPosition))
        return false;
    const auto caughtUp = now - std::chrono::milliseconds(BUILD_CATCH_UP_MS);
    mLastBuild = speed <= 0.0f ? now : std::max(due, caughtUp);
    return true;
}

// sub_141031D90 each tick: the block in view again (sub_14376B8D0 needs a block hit)
void SimulatedPlayerController::_continueBuild(ServerNetworkHandler &owner, ServerPlayer &player) {
    ScriptRaycast::BlockOptions ray;
    ray.mMaxDistance = BUILD_REACH;
    ScriptRaycast::BlockHit hit;
    if (ScriptRaycast::castBlocks(owner.getLevelFor(player), eyesOf(player), player.getLookDirection(), ray, hit))
        _buildOn(owner, player, hit.mBlock, faceOf(hit.mFace), hit.mFaceLocation, true);
}

void SimulatedPlayerController::startBuild(ServerNetworkHandler &owner, ServerPlayer &player, int32_t slot) {
    if (!player.isAlive()) {
        mBuilding = true;
        mHasBuilt = false;
        return;
    }
    // Player::setSelectedSlot (sub_1402DC830)
    PlayerInventory &inventory = player.getInventory();
    if (slot >= 0 && slot < PlayerInventory::HOTBAR_SIZE) {
        inventory.setSelectedSlot(slot);
        player.getInventoryManager().syncSelectedHotbarSlot();
    }
    // sub_141031E80: the block in view, else the liquid in view
    Level &level = owner.getLevelFor(player);
    ScriptRaycast::BlockOptions ray;
    ray.mMaxDistance = BUILD_REACH;
    ScriptRaycast::BlockHit hit;
    bool found = ScriptRaycast::castBlocks(level, eyesOf(player), player.getLookDirection(), ray, hit);
    bool liquid = false;
    if (!found) {
        ray.mIncludeLiquids = true;
        liquid = found = ScriptRaycast::castBlocks(level, eyesOf(player), player.getLookDirection(), ray, hit);
    }
    const int32_t face = faceOf(hit.mFace);
    // a block whose placed side leaves the dimension's height range stops it, a liquid never does
    const int32_t placedY = hit.mBlock.y + FACE_Y[face];
    if (found && !liquid && (placedY < level.getMinY() || placedY > level.getMaxY()))
        return;
    mBuilding = true;
    mHasBuilt = false;
    // GameMode::startBuildBlock (vtable +40): the first use at once
    if (found)
        _buildOn(owner, player, hit.mBlock, face, hit.mFaceLocation, false);
}

// a walking player's path, the player's own size
bool SimulatedPlayerController::_findPath(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3f &target) {
    Path path;
    const ActorSize size{MovementPhysics::PLAYER_WIDTH, MovementPhysics::PLAYER_HEIGHT};
    mWaypoints.clear();
    mWaypoint = 0;
    mStuckTicks = 0;
    mLastNavigationPosition = player.getPosition();
    mNavigationTarget = target;
    if (!PathFinder::get().findPath(owner.getLevelFor(player), player, size, target, PathOptions(), path))
        return false;
    mWaypoints = path.getWaypoints();
    return path.reachesTarget();
}

bool SimulatedPlayerController::navigateToLocation(ServerNetworkHandler &owner, ServerPlayer &player,
                                                   const Vector3f &target, float speed, std::vector<Vector3i> &path,
                                                   bool &fullPath) {
    mRoute.clear();
    mFollowEntity = 0;
    mSpeed = speed;
    fullPath = _findPath(owner, player, target);
    mMove = mWaypoints.empty() ? Move::None : Move::Navigate;
    path.clear();
    // sub_1410370A0: the path's nodes are block positions
    for (const Vector3f &waypoint: mWaypoints)
        path.emplace_back((int32_t) std::floor(waypoint.x), (int32_t) std::floor(waypoint.y),
                          (int32_t) std::floor(waypoint.z));
    return !mWaypoints.empty();
}

bool SimulatedPlayerController::navigateToEntity(ServerNetworkHandler &owner, ServerPlayer &player, int64_t uniqueId,
                                                 const Vector3f &target, float speed, std::vector<Vector3i> &path,
                                                 bool &fullPath) {
    const bool found = navigateToLocation(owner, player, target, speed, path, fullPath);
    mFollowEntity = uniqueId;
    mMove = Move::Navigate;
    return found;
}

void SimulatedPlayerController::navigateToLocations(const std::vector<Vector3f> &route, float speed) {
    mFollowEntity = 0;
    mSpeed = speed;
    mRoute = route;
    mRouteIndex = 0;
    mWaypoints.clear();
    mWaypoint = 0;
    mMove = route.empty() ? Move::None : Move::Navigate;
}

// sub_141035590 state 3 and 4: the path walked node after node, stopping when stuck
void SimulatedPlayerController::_tickNavigation(ServerNetworkHandler &owner, ServerPlayer &player, float &sideways,
                                                float &forward, float &vertical) {
    if (mFollowEntity != 0) {
        Vector3f target;
        if (!findEntity(owner, mFollowEntity, target, false)) {
            mMove = Move::None;
            return;
        }
        const Vector3f &here = player.getPosition();
        const float mx = target.x - mNavigationTarget.x;
        const float mz = target.z - mNavigationTarget.z;
        const float px = target.x - here.x;
        const float pz = target.z - here.z;
        const float follow = FOLLOW_DISTANCE * FOLLOW_DISTANCE;
        if (mx * mx + mz * mz > follow || (mWaypoint >= mWaypoints.size() && px * px + pz * pz > follow))
            _findPath(owner, player, target);
        if (mWaypoint >= mWaypoints.size())
            return;
    }
    while (mWaypoint >= mWaypoints.size()) {
        if (mFollowEntity != 0)
            return;
        if (mRouteIndex >= mRoute.size()) {
            mMove = Move::None;
            return;
        }
        _findPath(owner, player, mRoute[mRouteIndex++]);
    }
    const Vector3f &feet = player.getPosition();
    const Vector3f &waypoint = mWaypoints[mWaypoint];
    const float dx = waypoint.x - feet.x;
    const float dz = waypoint.z - feet.z;
    if (dx * dx + dz * dz < ARRIVAL_DISTANCE * ARRIVAL_DISTANCE && std::fabs(waypoint.y - feet.y) < 1.0f) {
        mWaypoint++;
        return;
    }
    _stepTowards(player, waypoint, mSpeed, true, sideways, forward, vertical);
    const bool onGround = player.hasSimulatedGround() ? player.isSimulatedOnGround() : player.isOnGround();
    if (waypoint.y - feet.y >= 0.5f && onGround)
        mJump = true;

    const float mx = feet.x - mLastNavigationPosition.x;
    const float mz = feet.z - mLastNavigationPosition.z;
    mLastNavigationPosition = feet;
    if (mx * mx + mz * mz > NAVIGATION_STUCK_DISTANCE_SQUARED) {
        mStuckTicks = 0;
        return;
    }
    if (++mStuckTicks >= NAVIGATION_STUCK_TICKS)
        mMove = Move::None;
}
