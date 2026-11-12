#include "World/Actor/Player/SimulatedPlayerController.h"

#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/PlayerAuthInputPacket.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"

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
    const bool sprinting = player.getFlags().get(ActorFlag::Sprinting);
    if (mSprinting) {
        packet.mInputData.push_back((int32_t) PlayerAuthInputData::Sprinting);
        if (!sprinting)
            packet.mInputData.push_back((int32_t) PlayerAuthInputData::StartSprinting);
    } else if (sprinting) {
        packet.mInputData.push_back((int32_t) PlayerAuthInputData::StopSprinting);
    }
}
