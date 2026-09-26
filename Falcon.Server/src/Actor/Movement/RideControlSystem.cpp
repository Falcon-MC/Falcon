#include "Actor/Movement/RideControlSystem.h"

#include "Actor/ActorFlags.h"
#include "Actor/AI/Goal/BehaviorItems.h"
#include "Actor/Mob/MobActor.h"
#include "Actor/Movement/ActorPushSystem.h"
#include "Actor/Movement/PreMoveTravelVelocitySystem.h"
#include "Actor/RideSystem.h"
#include "Actor/ServerPlayer.h"
#include "Block/Block.h"
#include "Inventory/InventoryManager.h"
#include "Block/Blocks/LiquidView.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Item/ItemDurability.h"
#include "Item/Loot/LegacyItemMapper.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/PlayerAuthInputPacket.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace {
    const char *const INPUT_GROUND_CONTROLLED = "minecraft:input_ground_controlled";
    const char *const FREE_CAMERA_CONTROLLED = "minecraft:free_camera_controlled";
    const char *const ITEM_CONTROLLABLE = "minecraft:item_controllable";
    const char *const CONTROLLED_BY_PLAYER = "minecraft:behavior.controlled_by_player";
    const char *const UNDERWATER_MOVEMENT = "minecraft:underwater_movement";
    const char *const FLYING_SPEED = "minecraft:flying_speed";
    const char *const IS_SADDLED = "minecraft:is_saddled";
    const char *const IS_CHESTED = "minecraft:is_chested";
    const char *const CAN_POWER_JUMP = "minecraft:can_power_jump";
    const char *const DASH_ACTION = "minecraft:dash_action";
    const char *const BOOSTABLE = "minecraft:boostable";
    const char *const ROTATION_AXIS_ALIGNED = "minecraft:rotation_axis_aligned";
    const char *const IS_COLLIDABLE = "minecraft:is_collidable";
    const char *const BODY_ROTATION_ALWAYS_FOLLOWS_HEAD = "minecraft:body_rotation_always_follows_head";
    const char *const VERTICAL_MOVEMENT_ACTION = "minecraft:vertical_movement_action";
    const char *const DASH_DIRECTION_ENTITY = "entity";
    const char *const DASH_DIRECTION_PASSENGER = "passenger";

    const float DEGREES_TO_RADIANS = 0.017453292519943295f;
    const int32_t TICKS_PER_SECOND = 20;
    const float NORMAL_FRICTION = 0.6f;
    const float SLIPPERY_THRESHOLD = 0.05f;
    const float MOTION_EPSILON_SQUARED = 0.000025f;

    const float GROUND_DEADZONE = 0.08f;
    const float GROUND_CURVE_EXPONENT = 1.6f;
    const float GROUND_ACCELERATION = 0.30f;
    const float GROUND_BRAKE = 0.45f;
    const float GROUND_SPEED_KNOB = 1.80f;
    const float GROUND_BACKWARDS_MODIFIER = 0.5f;
    const int64_t JUMP_CONTROL_GRACE_TICKS = 5;

    const float AIR_HORIZONTAL_TUNE = 0.97f;
    const float AIR_VERTICAL_TUNE = 0.60f;
    const float AIR_SPEED_KNOB = 7.0f;
    const float AIR_INPUT_EPSILON = 0.01f;
    const float MAX_PITCH = 80.0f;
    const int32_t AIR_ROTATION_DELAY_TICKS = 1;
    const float AIR_YAW_TURN_SPEED = 5.625f;
    const float AIR_YAW_INPUT_THRESHOLD = 2.0f;
    const float AIR_YAW_STOP_DISTANCE = 1.25f;

    const float WATER_SPEED_KNOB = 2.40f;
    const float WATER_DASH_DRAG = 0.88f;
    const float WATER_OUT_EXTRA_GRAVITY = 0.05f;
    const float WATER_MAX_UPWARD_OUT = 0.35f;
    const float WATER_BREACH_EPSILON = 0.25f;
    const float WATER_SUBMERGED_RATIO = 0.65f;
    const float WATER_DRIVE_GRAVITY_BIAS = 0.01f;
    const float WATER_MAX_DOWNWARD = -0.10f;
    const float WATER_DASH_INPUT_BLEND = 0.35f;
    const float WATER_DASH_EPSILON_SQUARED = 0.0004f;
    const int32_t WATER_COLUMN_MARGIN = 2;

    const float DEFAULT_STRAFE_MODIFIER = 0.4f;
    const float DEFAULT_BACKWARDS_MODIFIER = 0.5f;

    const float CHARGE_TICKS = 10.0f;
    const float MIN_JUMP_HEIGHT = 1.0f;
    const float MAX_JUMP_HEIGHT = 5.5f;
    const float MIN_JUMP_STRENGTH = 0.4f;
    const float JUMP_STRENGTH_SPAN = 0.6f;
    const int32_t JUMP_SOLVER_ITERATIONS = 30;
    const int32_t JUMP_SIMULATION_TICKS = 60;

    const float DASH_MIN_CHARGE = 0.05f;
    const float DASH_CHARGE_EXPONENT = 1.40f;
    const float DASH_CURVE_EXPONENT = 1.10f;
    const float DASH_HORIZONTAL_SCALE = 0.026f;
    const float DASH_VERTICAL_SCALE = 0.61355f;
    const float DASH_WATER_HORIZONTAL_SCALE = 0.40f;
    const float DASH_WATER_VERTICAL_SCALE = 0.08f;
    const float DASH_DEFAULT_COOLDOWN = 1.0f;
    const float DASH_DEFAULT_HORIZONTAL = 1.0f;
    const float DASH_DEFAULT_VERTICAL = 0.1f;

    const float DEFAULT_BOOST_MULTIPLIER = 1.35f;
    const float DEFAULT_BOOST_SECONDS = 3.0f;

    float numberIn(const json::Value *component, const char *key, float fallback) {
        const json::Value *value = component == nullptr ? nullptr : component->get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }

    float componentValue(const MobActor &mob, const char *component, float fallback) {
        return numberIn(mob.getComponent(component), "value", fallback);
    }

    float clamp01(float value) {
        return std::max(0.0f, std::min(1.0f, value));
    }

    float wrapDegrees(float angle) {
        return angle - 360.0f * std::floor((angle + 180.0f) / 360.0f);
    }

    float approachDegrees(float current, float target, float maxStep) {
        const float delta = std::max(-maxStep, std::min(maxStep, wrapDegrees(target - current)));
        return current + delta;
    }

    void setRideFlag(ServerNetworkHandler &owner, MobActor &mob, ActorFlag flag, bool value) {
        if (mob.getFlags().get(flag) == value)
            return;

        mob.getFlags().set(flag, value);
        owner.syncActorFlags(mob);
    }

    float groundFriction(Level &level, const MobActor &mob) {
        const AxisAlignedBB box = ActorPushSystem::boundingBoxOf(mob);
        const int32_t minX = (int32_t) std::floor(box.mMinX + 0.05f);
        const int32_t maxX = (int32_t) std::floor(box.mMaxX - 0.05f);
        const int32_t minZ = (int32_t) std::floor(box.mMinZ + 0.05f);
        const int32_t maxZ = (int32_t) std::floor(box.mMaxZ - 0.05f);
        const int32_t y = (int32_t) std::floor(box.mMinY - 0.05f);

        float best = NORMAL_FRICTION;
        for (int32_t x = minX; x <= maxX; ++x) {
            for (int32_t z = minZ; z <= maxZ; ++z) {
                const BlockState *state = level.peekBlockPtr(x, y, z);
                if (state == nullptr)
                    continue;

                const float friction = Block(*state).getFrictionFactor();
                if (std::isfinite(friction) && friction > best)
                    best = friction;
            }
        }
        return best;
    }

    float slipperinessOf(float friction) {
        if (friction <= NORMAL_FRICTION)
            return 0.0f;
        return clamp01((friction - NORMAL_FRICTION) / (1.0f - NORMAL_FRICTION));
    }

    float surfaceSpeedFactor(float friction) {
        if (friction <= NORMAL_FRICTION + SLIPPERY_THRESHOLD)
            return 1.0f;
        return 1.0f + slipperinessOf(friction) * 0.85f;
    }

    float brakeFactor(float friction) {
        if (friction <= NORMAL_FRICTION)
            return GROUND_BRAKE;

        const float brake = 0.018f - slipperinessOf(friction) * 0.014f;
        return std::max(0.004f, std::min(GROUND_BRAKE, brake));
    }

    bool touchesWater(Level &level, const MobActor &mob) {
        return LiquidBlocksFetch::at(level, mob.getPosition()).water;
    }

    bool topWaterY(Level &level, const MobActor &mob, int32_t &topY) {
        const Vector3f position = mob.getPosition();
        const int32_t x = (int32_t) std::floor(position.x);
        const int32_t z = (int32_t) std::floor(position.z);
        const int32_t minY = (int32_t) std::floor(position.y) - WATER_COLUMN_MARGIN;
        const int32_t maxY = (int32_t) std::floor(position.y + mob.getSize().mHeight) + WATER_COLUMN_MARGIN;

        for (int32_t y = maxY; y >= minY; --y) {
            const BlockState *state = level.peekBlockPtr(x, y, z);
            if (state != nullptr && LiquidView(*state).isWater()) {
                topY = y;
                return true;
            }
        }
        return false;
    }

    bool inWaterForDash(Level &level, const MobActor &mob) {
        int32_t topY = 0;
        return touchesWater(level, mob) || topWaterY(level, mob, topY);
    }

    float simulateJumpHeight(float motionY, float gravity) {
        float y = 0.0f;
        float maxY = 0.0f;
        for (int32_t tick = 0; tick < JUMP_SIMULATION_TICKS; ++tick) {
            motionY = (motionY - gravity) * PreMoveTravelVelocitySystem::AIR_FRICTION;
            y += motionY;
            if (y > maxY)
                maxY = y;
            if (motionY < 0.0f && y < maxY - 0.01f)
                break;
        }
        return maxY;
    }

    float solveJumpMotion(float height, float gravity) {
        float low = 0.05f;
        float high = 2.0f;
        while (simulateJumpHeight(high, gravity) < height && high < 10.0f)
            high *= 1.5f;

        for (int32_t iteration = 0; iteration < JUMP_SOLVER_ITERATIONS; ++iteration) {
            const float middle = (low + high) * 0.5f;
            if (simulateJumpHeight(middle, gravity) >= height)
                high = middle;
            else
                low = middle;
        }
        return high;
    }

    std::string itemIdentifier(const std::string &name) {
        return LegacyItemMapper::getInstance().resolve(name, 0);
    }
}

RideControlType RideControlSystem::controlTypeOf(const MobActor &mob) {
    if (mob.getComponent(ITEM_CONTROLLABLE) != nullptr)
        return RideControlType::Item;

    if (mob.getComponent(INPUT_GROUND_CONTROLLED) != nullptr)
        return RideControlType::Ground;

    if (mob.getComponent(FREE_CAMERA_CONTROLLED) != nullptr)
        return mob.getComponent(UNDERWATER_MOVEMENT) != nullptr ? RideControlType::Water : RideControlType::Air;

    return RideControlType::None;
}

bool RideControlSystem::isControlledBy(const MobActor &mob, const ServerPlayer &player) {
    const std::vector<int64_t> &passengers = mob.getPassengers();
    return !passengers.empty() && passengers.front() == player.getUniqueId();
}

ServerPlayer *RideControlSystem::controllerOf(ServerNetworkHandler &owner, const MobActor &mob) {
    const std::vector<int64_t> &passengers = mob.getPassengers();
    if (passengers.empty())
        return nullptr;

    return dynamic_cast<ServerPlayer *>(RideSystem::resolve(owner, passengers.front()));
}

void RideControlSystem::receiveInput(ServerNetworkHandler &owner, ServerPlayer &player,
                                     const PlayerAuthInputPacket &packet) {
    MobActor *mob = dynamic_cast<MobActor *>(RideSystem::resolve(owner, player.getVehicleId()));
    if (mob == nullptr || !isControlledBy(*mob, player))
        return;

    const auto has = [&packet](PlayerAuthInputData flag) {
        return packet.hasInputFlag((int32_t) flag);
    };

    RiderInput &input = mob->getRideControl().mInput;
    input.mStrafe = std::isfinite(packet.mMotionX) ? packet.mMotionX : 0.0f;
    input.mForward = std::isfinite(packet.mMotionY) ? packet.mMotionY : 0.0f;
    input.mRawStrafe = std::isfinite(packet.mRawMoveVectorX) ? packet.mRawMoveVectorX : 0.0f;
    input.mRawForward = std::isfinite(packet.mRawMoveVectorY) ? packet.mRawMoveVectorY : 0.0f;
    input.mPitch = std::isfinite(packet.mInteractRotationX) ? packet.mInteractRotationX : 0.0f;
    input.mYaw = std::isfinite(packet.mInteractRotationY) ? packet.mInteractRotationY : 0.0f;
    input.mJumpHeld = has(PlayerAuthInputData::WantUp) || has(PlayerAuthInputData::JumpDown)
                      || has(PlayerAuthInputData::Jumping) || has(PlayerAuthInputData::StartJumping);
    input.mSprinting = has(PlayerAuthInputData::SprintDown) || has(PlayerAuthInputData::Sprinting)
                       || has(PlayerAuthInputData::StartSprinting);
    input.mReceived = true;
}

bool RideControlSystem::tick(ServerNetworkHandler &owner, MobActor &mob) {
    _tickCooldowns(owner, mob);

    const RideControlType type = controlTypeOf(mob);
    if (type == RideControlType::None)
        return false;

    const ServerPlayer *rider = controllerOf(owner, mob);
    if (rider == nullptr)
        return false;

    if (type == RideControlType::Item && !_holdsControlItem(mob, *rider))
        return false;

    switch (type) {
        case RideControlType::Ground:
            if (!_handleJumpOrDash(owner, mob, type))
                _tickGround(owner, mob);
            break;
        case RideControlType::Water:
            if (!_handleJumpOrDash(owner, mob, type))
                _tickWater(owner, mob);
            break;
        case RideControlType::Air:
            _tickAir(mob);
            break;
        case RideControlType::Item:
            _tickItem(mob);
            break;
        case RideControlType::None:
            break;
    }
    return true;
}

bool RideControlSystem::_holdsControlItem(const MobActor &mob, const ServerPlayer &player) {
    const json::Value *controllable = mob.getComponent(ITEM_CONTROLLABLE);
    if (controllable == nullptr)
        return false;

    return BehaviorItems(controllable->get("control_items")).contains(player.getInventory().getItemInHand());
}

void RideControlSystem::_tickCooldowns(ServerNetworkHandler &owner, MobActor &mob) {
    RideControlState &state = mob.getRideControl();
    const int64_t now = owner.getCurrentTick();

    if (state.mDashCooldownEndTick != -1 && now >= state.mDashCooldownEndTick) {
        state.mDashCooldownEndTick = -1;
        setRideFlag(owner, mob, ActorFlag::HasDashCooldown, false);
    }

    if (state.mRearingTicks > 0 && --state.mRearingTicks == 0)
        setRideFlag(owner, mob, ActorFlag::Rearing, false);

    if (state.mJumpTick != -1 && mob.isOnGround() && now - state.mJumpTick > JUMP_CONTROL_GRACE_TICKS) {
        state.mJumpTick = -1;
        if (state.mRearingTicks == 0)
            setRideFlag(owner, mob, ActorFlag::Rearing, false);
    }

    if (state.mBoostTicks > 0)
        state.mBoostTicks--;
}

bool RideControlSystem::_handleJumpOrDash(ServerNetworkHandler &owner, MobActor &mob, RideControlType type) {
    RideControlState &state = mob.getRideControl();
    const bool jumpHeld = state.mInput.mJumpHeld;
    Level &level = owner.getLevelFor(mob);

    if (mob.getComponent(CAN_POWER_JUMP) != nullptr) {
        if (jumpHeld) {
            if (!mob.isOnGround())
                return false;

            state.mJumpChargeTicks++;
            return true;
        }

        if (state.mJumpChargeTicks == -1)
            return false;

        const float chargeTicks = (float) state.mJumpChargeTicks;
        state.mJumpChargeTicks = -1;
        if (!mob.isOnGround())
            return false;

        const float charge = std::min(chargeTicks / CHARGE_TICKS, 1.0f);
        const float strength = clamp01((mob.getJumpStrength() - MIN_JUMP_STRENGTH) / JUMP_STRENGTH_SPAN);
        const float height = MIN_JUMP_HEIGHT + (MAX_JUMP_HEIGHT - MIN_JUMP_HEIGHT) * strength;
        const float lift = solveJumpMotion(height, mob.getPhysics().mGravity) * charge;

        const Vector3f motion = mob.getMotion();
        mob.setMotion(Vector3f(motion.x, lift, motion.z));
        state.mJumpTick = owner.getCurrentTick();
        setRideFlag(owner, mob, ActorFlag::Rearing, true);
        return true;
    }

    if (mob.getComponent(DASH_ACTION) == nullptr)
        return false;

    if (jumpHeld) {
        if (state.mDashCooldownEndTick != -1)
            return false;
        if (type == RideControlType::Ground && !mob.isOnGround())
            return false;

        if (state.mDashChargeTicks == -1) {
            state.mDashChargeTicks = 0;
            if (type == RideControlType::Water)
                state.mWaterDashStartedInWater = inWaterForDash(level, mob);
        }
        state.mDashChargeTicks++;
        return true;
    }

    if (state.mDashChargeTicks == -1)
        return false;

    const float charge = std::min((float) state.mDashChargeTicks / CHARGE_TICKS, 1.0f);
    state.mDashChargeTicks = -1;

    if (type == RideControlType::Water) {
        const bool allowed = state.mWaterDashStartedInWater || inWaterForDash(level, mob);
        state.mWaterDashStartedInWater = false;
        if (!allowed)
            return true;

        const json::Value *underwater = mob.getComponent(DASH_ACTION)->get("can_dash_underwater");
        if ((underwater == nullptr || !underwater->boolean(false)) && touchesWater(level, mob))
            return true;
    }

    return _tryDash(owner, mob, type, charge);
}

bool RideControlSystem::_tryDash(ServerNetworkHandler &owner, MobActor &mob, RideControlType type, float charge) {
    const json::Value *dash = mob.getComponent(DASH_ACTION);
    RideControlState &state = mob.getRideControl();
    if (dash == nullptr || state.mDashCooldownEndTick != -1)
        return false;

    Level &level = owner.getLevelFor(mob);
    const json::Value *underwater = dash->get("can_dash_underwater");
    if (touchesWater(level, mob) && (underwater == nullptr || !underwater->boolean(false)))
        return false;

    charge = clamp01(charge);
    if (charge < DASH_MIN_CHARGE)
        return false;

    const json::Value *directionValue = dash->get("direction");
    const std::string direction = directionValue == nullptr ? DASH_DIRECTION_ENTITY : directionValue->string();
    const bool entityDirection = direction == DASH_DIRECTION_ENTITY;
    const float yaw = (entityDirection ? mob.getRotation().y : state.mInput.mYaw) * DEGREES_TO_RADIANS;
    const float pitch = (direction == DASH_DIRECTION_PASSENGER ? state.mInput.mPitch : 0.0f) * DEGREES_TO_RADIANS;

    float x = -std::sin(yaw);
    float y = 0.0f;
    float z = std::cos(yaw);
    if (!entityDirection) {
        x *= std::cos(pitch);
        y = -std::sin(pitch);
        z *= std::cos(pitch);
    }

    const float length = std::sqrt(x * x + y * y + z * z);
    if (length < 1.0e-9f)
        return false;
    x /= length;
    y /= length;
    z /= length;

    const float scaledCharge = std::pow(charge, DASH_CHARGE_EXPONENT);
    const float curve = std::pow(scaledCharge, DASH_CURVE_EXPONENT);
    const float horizontalMomentum = numberIn(dash, "horizontal_momentum", DASH_DEFAULT_HORIZONTAL);
    const float verticalMomentum = numberIn(dash, "vertical_momentum", DASH_DEFAULT_VERTICAL);

    float horizontal = std::min(horizontalMomentum * scaledCharge * DASH_HORIZONTAL_SCALE * curve,
                                horizontalMomentum * DASH_HORIZONTAL_SCALE);
    const float verticalImpulse = verticalMomentum * DASH_VERTICAL_SCALE * scaledCharge;
    float vertical = entityDirection ? verticalImpulse : y * horizontal + verticalImpulse;

    if (type == RideControlType::Water && inWaterForDash(level, mob)) {
        horizontal *= DASH_WATER_HORIZONTAL_SCALE;
        vertical *= DASH_WATER_VERTICAL_SCALE;
    }

    const Vector3f motion = mob.getMotion();
    mob.setMotion(Vector3f(motion.x + x * horizontal, motion.y + vertical, motion.z + z * horizontal));

    const float cooldown = std::max(0.0f, numberIn(dash, "cooldown_time", DASH_DEFAULT_COOLDOWN));
    const int32_t ticks = (int32_t) std::ceil(cooldown * (float) TICKS_PER_SECOND);
    if (ticks > 0) {
        state.mDashCooldownEndTick = owner.getCurrentTick() + ticks;
        setRideFlag(owner, mob, ActorFlag::HasDashCooldown, true);
    }
    return true;
}

void RideControlSystem::_tickGround(ServerNetworkHandler &owner, MobActor &mob) {
    RideControlState &state = mob.getRideControl();
    const RiderInput &input = state.mInput;
    Level &level = owner.getLevelFor(mob);

    const float friction = groundFriction(level, mob);
    const bool slippery = !touchesWater(level, mob) && friction > NORMAL_FRICTION + SLIPPERY_THRESHOLD;
    const float slipperiness = slipperinessOf(friction);

    Vector3f motion = mob.getMotion();
    if (slippery) {
        if (!state.mWasOnSlipperyGround) {
            state.mSlipperyEntrySpeed = std::sqrt(motion.x * motion.x + motion.z * motion.z);
            state.mSlipperyGraceTicks = 16;
        } else if (state.mSlipperyGraceTicks > 0) {
            state.mSlipperyGraceTicks--;
        }
        state.mWasOnSlipperyGround = true;
    } else {
        state.mWasOnSlipperyGround = false;
        state.mSlipperyGraceTicks = 0;
        state.mSlipperyEntrySpeed = 0.0f;
    }

    Vector3f rotation = mob.getRotation();
    rotation.y = input.mYaw;
    rotation.z = input.mYaw;
    mob.setRotation(rotation);

    const bool jumpGrace = state.mJumpTick != -1 && owner.getCurrentTick() - state.mJumpTick <= JUMP_CONTROL_GRACE_TICKS;
    if (!mob.isOnGround() && !jumpGrace)
        return;

    const float inX = input.mStrafe;
    const float inY = input.mForward < 0.0f ? input.mForward * GROUND_BACKWARDS_MODIFIER : input.mForward;
    const float magnitude = std::sqrt(inX * inX + inY * inY);

    if (magnitude <= GROUND_DEADZONE) {
        if (slippery)
            return;

        const float brake = brakeFactor(friction);
        float newX = motion.x * (1.0f - brake);
        float newZ = motion.z * (1.0f - brake);
        if (newX * newX + newZ * newZ < MOTION_EPSILON_SQUARED) {
            newX = 0.0f;
            newZ = 0.0f;
        }
        mob.setMotion(Vector3f(newX, motion.y, newZ));
        return;
    }

    const float dirX = inX / magnitude;
    const float dirY = inY / magnitude;
    const float yaw = input.mYaw * DEGREES_TO_RADIANS;
    const float wishX = -std::sin(yaw) * dirY + std::cos(yaw) * dirX;
    const float wishZ = std::cos(yaw) * dirY + std::sin(yaw) * dirX;

    float strength = std::min(magnitude, 1.0f);
    strength = std::max(0.0f, (strength - GROUND_DEADZONE) / (1.0f - GROUND_DEADZONE));
    strength = std::pow(strength, GROUND_CURVE_EXPONENT);

    float maxSpeed = mob.getMovementSpeed();
    if (input.mSprinting)
        maxSpeed *= mob.getRideSprintMultiplier();
    const float cap = maxSpeed * surfaceSpeedFactor(friction) / GROUND_SPEED_KNOB;

    if (slippery) {
        _applySlipperyLookInput(mob, wishX, wishZ, strength, cap, slipperiness);
        return;
    }

    float newX = motion.x + (wishX * cap * strength - motion.x) * GROUND_ACCELERATION;
    float newZ = motion.z + (wishZ * cap * strength - motion.z) * GROUND_ACCELERATION;
    const float speedSquared = newX * newX + newZ * newZ;
    if (speedSquared > cap * cap) {
        const float scale = cap / std::sqrt(speedSquared);
        newX *= scale;
        newZ *= scale;
    }
    mob.setMotion(Vector3f(newX, motion.y, newZ));
}

void RideControlSystem::_applySlipperyLookInput(MobActor &mob, float wishX, float wishZ, float strength, float cap,
                                                float slipperiness) {
    const Vector3f motion = mob.getMotion();
    const float speedSquared = motion.x * motion.x + motion.z * motion.z;
    if (speedSquared < MOTION_EPSILON_SQUARED) {
        _applySlipperyImpulse(mob, wishX, wishZ, strength, cap, 1.0f, slipperiness);
        return;
    }

    const float speed = std::sqrt(speedSquared);
    const float moveX = motion.x / speed;
    const float moveZ = motion.z / speed;
    const float sideX = -moveZ;
    const float sideZ = moveX;
    const float forwardDot = wishX * moveX + wishZ * moveZ;
    const float sideDot = wishX * sideX + wishZ * sideZ;
    const float speedRatio = cap <= 0.0f ? 1.0f : clamp01(speed / cap);

    float forwardControl = forwardDot;
    if (forwardDot < 0.0f) {
        float reverseAuthority = (0.10f - slipperiness * 0.06f) * (1.0f - speedRatio * 0.65f);
        reverseAuthority = std::max(0.015f, reverseAuthority);
        forwardControl = forwardDot * reverseAuthority;
    }

    float sideAuthority = std::max(0.10f, 0.38f - slipperiness * 0.22f);
    if (cap > 0.0f && speed / cap > 1.0f)
        sideAuthority *= 0.55f;

    float mergedX = moveX * forwardControl + sideX * sideDot * sideAuthority;
    float mergedZ = moveZ * forwardControl + sideZ * sideDot * sideAuthority;
    const float mergedLength = std::sqrt(mergedX * mergedX + mergedZ * mergedZ);
    if (mergedLength < 0.000001f)
        return;

    mergedX /= mergedLength;
    mergedZ /= mergedLength;
    const float controlScale = clamp01(std::fabs(forwardControl) + std::fabs(sideDot) * sideAuthority);
    _applySlipperyImpulse(mob, mergedX, mergedZ, strength, cap, controlScale, slipperiness);
}

void RideControlSystem::_applySlipperyImpulse(MobActor &mob, float wishX, float wishZ, float strength, float cap,
                                              float controlScale, float slipperiness) {
    const RideControlState &state = mob.getRideControl();
    const Vector3f motion = mob.getMotion();
    const float speed = std::sqrt(motion.x * motion.x + motion.z * motion.z);
    const float speedRatio = cap <= 0.0f ? 0.0f : clamp01(speed / cap);
    const float traction = std::max(0.035f, 0.20f - slipperiness * 0.145f);
    const float lowSpeedLimiter = 0.14f + speedRatio * 0.86f;

    float impulse = cap * 0.075f * strength * traction * controlScale * lowSpeedLimiter;
    if (speed > cap)
        impulse *= 0.22f;

    float newX = motion.x + wishX * impulse;
    float newZ = motion.z + wishZ * impulse;
    const float newSpeed = std::sqrt(newX * newX + newZ * newZ);

    float maxSlideSpeed = cap * (1.50f + slipperiness * 0.65f);
    if (state.mSlipperyGraceTicks > 0 && state.mSlipperyEntrySpeed > maxSlideSpeed)
        maxSlideSpeed = state.mSlipperyEntrySpeed;

    const float gain = speed < cap * 0.35f ? 0.014f : 0.0035f;
    const float allowedSpeed = std::min(maxSlideSpeed, std::max(speed, speed + cap * gain * strength * controlScale));
    if (newSpeed > allowedSpeed && newSpeed > 0.000001f) {
        const float scale = allowedSpeed / newSpeed;
        newX *= scale;
        newZ *= scale;
    }
    mob.setMotion(Vector3f(newX, motion.y, newZ));
}

void RideControlSystem::_applyAirRotation(MobActor &mob, float targetYaw) {
    RideControlState &state = mob.getRideControl();
    Vector3f rotation = mob.getRotation();

    if (!state.mAirRotationInitialized) {
        state.mAirRotationInitialized = true;
        state.mAirTargetYaw = rotation.y;
        state.mAirRotationDelayTicks = 0;
    }

    if (std::fabs(wrapDegrees(state.mAirTargetYaw - rotation.y)) <= AIR_YAW_STOP_DISTANCE
        && std::fabs(wrapDegrees(targetYaw - state.mAirTargetYaw)) > AIR_YAW_INPUT_THRESHOLD) {
        state.mAirTargetYaw = targetYaw;
        state.mAirRotationDelayTicks = AIR_ROTATION_DELAY_TICKS;
    }

    if (state.mAirRotationDelayTicks > 0) {
        state.mAirRotationDelayTicks--;
        return;
    }

    const float yaw = approachDegrees(rotation.y, state.mAirTargetYaw, AIR_YAW_TURN_SPEED);
    rotation.x = 0.0f;
    rotation.y = yaw;
    rotation.z = yaw;
    mob.setRotation(rotation);
}

void RideControlSystem::_tickAir(MobActor &mob) {
    const RiderInput &input = mob.getRideControl().mInput;
    _applyAirRotation(mob, input.mYaw);

    const json::Value *camera = mob.getComponent(FREE_CAMERA_CONTROLLED);
    float forward = input.mRawForward;
    float strafe = input.mRawStrafe * numberIn(camera, "strafe_speed_modifier", DEFAULT_STRAFE_MODIFIER);
    if (forward < 0.0f)
        forward *= numberIn(camera, "backwards_movement_modifier", DEFAULT_BACKWARDS_MODIFIER);

    float speed = componentValue(mob, FLYING_SPEED, 0.0f) * AIR_SPEED_KNOB;
    if (input.mSprinting)
        speed *= mob.getRideSprintMultiplier();

    if (std::fabs(forward) < AIR_INPUT_EPSILON && std::fabs(strafe) < AIR_INPUT_EPSILON && !input.mJumpHeld) {
        mob.setMotion(Vector3f(0.0f, 0.0f, 0.0f));
        return;
    }

    const float yaw = mob.getRotation().y * DEGREES_TO_RADIANS;
    const float pitch = std::max(-MAX_PITCH, std::min(MAX_PITCH, input.mPitch)) * DEGREES_TO_RADIANS;
    const float dx = (-std::sin(yaw) * forward + std::cos(yaw) * strafe) * speed * AIR_HORIZONTAL_TUNE;
    const float dz = (std::cos(yaw) * forward + std::sin(yaw) * strafe) * speed * AIR_HORIZONTAL_TUNE;
    const float dy = input.mJumpHeld ? speed * AIR_VERTICAL_TUNE : -std::sin(pitch) * speed * AIR_VERTICAL_TUNE;
    mob.setMotion(Vector3f(dx, dy, dz));
}

void RideControlSystem::_tickWater(ServerNetworkHandler &owner, MobActor &mob) {
    const RideControlState &state = mob.getRideControl();
    const RiderInput &input = state.mInput;
    Level &level = owner.getLevelFor(mob);

    Vector3f rotation = mob.getRotation();
    rotation.x = input.mPitch;
    rotation.y = input.mYaw;
    rotation.z = input.mYaw;
    mob.setRotation(rotation);

    const Vector3f position = mob.getPosition();
    const float height = mob.getSize().mHeight;
    int32_t topY = 0;
    const bool hasSurface = topWaterY(level, mob, topY);
    const float surfaceY = (float) topY + 1.0f;
    const bool inWaterColumn = touchesWater(level, mob) || (hasSurface && position.y <= surfaceY + WATER_BREACH_EPSILON);

    const json::Value *camera = mob.getComponent(FREE_CAMERA_CONTROLLED);
    float forward = input.mRawForward;
    const float strafe = input.mRawStrafe * numberIn(camera, "strafe_speed_modifier", DEFAULT_STRAFE_MODIFIER);
    if (forward < 0.0f)
        forward *= numberIn(camera, "backwards_movement_modifier", DEFAULT_BACKWARDS_MODIFIER);

    float speed = componentValue(mob, UNDERWATER_MOVEMENT, 0.0f) * WATER_SPEED_KNOB;
    if (input.mSprinting)
        speed *= mob.getRideSprintMultiplier();

    Vector3f motion = mob.getMotion();
    const bool moving = std::fabs(forward) >= AIR_INPUT_EPSILON || std::fabs(strafe) >= AIR_INPUT_EPSILON;
    const float motionSquared = motion.x * motion.x + motion.y * motion.y + motion.z * motion.z;
    const bool dashing = state.mDashChargeTicks != -1
                         || (state.mDashCooldownEndTick != -1 && motionSquared > WATER_DASH_EPSILON_SQUARED);

    if (!moving && inWaterColumn && !dashing) {
        mob.setMotion(Vector3f(0.0f, 0.0f, 0.0f));
        return;
    }

    if (!moving && dashing) {
        if (inWaterColumn) {
            motion.x *= WATER_DASH_DRAG;
            motion.z *= WATER_DASH_DRAG;
        } else {
            motion.y = std::min(motion.y, WATER_MAX_UPWARD_OUT) - WATER_OUT_EXTRA_GRAVITY;
        }
        mob.setMotion(motion);
        return;
    }

    const float yaw = input.mYaw * DEGREES_TO_RADIANS;
    const float pitch = std::max(-MAX_PITCH, std::min(MAX_PITCH, input.mPitch)) * DEGREES_TO_RADIANS;
    float desiredX = (-std::sin(yaw) * forward + std::cos(yaw) * strafe) * speed * AIR_HORIZONTAL_TUNE;
    float desiredZ = (std::cos(yaw) * forward + std::sin(yaw) * strafe) * speed * AIR_HORIZONTAL_TUNE;
    float desiredY = -std::sin(pitch) * speed * AIR_VERTICAL_TUNE - WATER_DRIVE_GRAVITY_BIAS;

    if (inWaterColumn && hasSurface) {
        const float maxFeetY = surfaceY - height * WATER_SUBMERGED_RATIO;
        if (position.y >= maxFeetY) {
            desiredY = std::min(desiredY, 0.0f);
            if (motion.y > 0.0f && !dashing)
                motion.y = 0.0f;
        } else if (desiredY > 0.0f) {
            desiredY = std::min(desiredY, std::max(0.0f, maxFeetY - position.y));
        }
    } else if (!inWaterColumn && desiredY > 0.0f) {
        desiredY = 0.0f;
    }

    if (!inWaterColumn) {
        motion.y = std::min(motion.y, WATER_MAX_UPWARD_OUT);
        if (dashing)
            motion.y -= WATER_OUT_EXTRA_GRAVITY;
        else
            desiredY -= WATER_OUT_EXTRA_GRAVITY;
    } else {
        motion.y = std::max(motion.y, WATER_MAX_DOWNWARD);
    }

    if (dashing) {
        desiredX = motion.x + (desiredX - motion.x) * WATER_DASH_INPUT_BLEND;
        desiredZ = motion.z + (desiredZ - motion.z) * WATER_DASH_INPUT_BLEND;
        desiredY = motion.y;
    }

    mob.setMotion(Vector3f(desiredX, desiredY, desiredZ));
}

void RideControlSystem::_tickItem(MobActor &mob) {
    const RideControlState &state = mob.getRideControl();
    const float yawDegrees = state.mInput.mYaw;

    Vector3f rotation = mob.getRotation();
    rotation.y = yawDegrees;
    rotation.z = yawDegrees;
    mob.setRotation(rotation);

    if (!mob.isOnGround())
        return;

    const json::Value *controlled = mob.getComponent(CONTROLLED_BY_PLAYER);
    float speed = mob.getMovementSpeed() * numberIn(controlled, "mount_speed_multiplier", 1.0f);
    if (state.mBoostTicks > 0)
        speed *= state.mBoostMultiplier;

    const float cap = speed / GROUND_SPEED_KNOB;
    const float yaw = yawDegrees * DEGREES_TO_RADIANS;
    const Vector3f motion = mob.getMotion();
    const float newX = motion.x + (-std::sin(yaw) * cap - motion.x) * GROUND_ACCELERATION;
    const float newZ = motion.z + (std::cos(yaw) * cap - motion.z) * GROUND_ACCELERATION;
    mob.setMotion(Vector3f(newX, motion.y, newZ));
}

bool RideControlSystem::tryBoost(ServerNetworkHandler &owner, ServerPlayer &player) {
    MobActor *mob = dynamic_cast<MobActor *>(RideSystem::resolve(owner, player.getVehicleId()));
    if (mob == nullptr || !isControlledBy(*mob, player))
        return false;

    const json::Value *boostable = mob->getComponent(BOOSTABLE);
    const json::Value *items = boostable == nullptr ? nullptr : boostable->get("boost_items");
    if (items == nullptr || !items->isArray())
        return false;

    RideControlState &state = mob->getRideControl();
    PlayerInventory &inventory = player.getInventory();
    ItemStack held = inventory.getItemInHand();

    for (const std::unique_ptr<json::Value> &entry: items->mArray) {
        if (!BehaviorItems(entry.get()).contains(held))
            continue;

        if (state.mBoostTicks > 0)
            return true;

        state.mBoostMultiplier = numberIn(boostable, "speed_multiplier", DEFAULT_BOOST_MULTIPLIER);
        state.mBoostTicks = (int32_t) std::lround(numberIn(boostable, "duration", DEFAULT_BOOST_SECONDS)
                                                  * (float) TICKS_PER_SECOND);

        if (player.getGameType() == (int32_t) GameType::Creative)
            return true;

        ItemDurability::apply(&player, held, (int32_t) numberIn(entry.get(), "damage", 1.0f));
        const json::Value *replacement = entry->get("replace_item");
        if (held.isAir() && replacement != nullptr)
            held = owner.createItemStack(itemIdentifier(replacement->string()), 1);

        inventory.setItemInHand(std::move(held));
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, inventory.getSelectedSlot());
        return true;
    }
    return false;
}

void RideControlSystem::syncFlags(ServerNetworkHandler &owner, MobActor &mob) {
    const RideControlType type = controlTypeOf(mob);
    const bool ground = type == RideControlType::Ground;
    const bool air = type == RideControlType::Air;
    const bool water = type == RideControlType::Water;

    setRideFlag(owner, mob, ActorFlag::Saddled, mob.getComponent(IS_SADDLED) != nullptr);
    setRideFlag(owner, mob, ActorFlag::Chested, mob.getComponent(IS_CHESTED) != nullptr);
    setRideFlag(owner, mob, ActorFlag::CanPowerJump, mob.getComponent(CAN_POWER_JUMP) != nullptr);
    setRideFlag(owner, mob, ActorFlag::CanDash, mob.getComponent(DASH_ACTION) != nullptr);
    setRideFlag(owner, mob, ActorFlag::RotationAxisAligned, mob.getComponent(ROTATION_AXIS_ALIGNED) != nullptr);
    setRideFlag(owner, mob, ActorFlag::Collidable, mob.getComponent(IS_COLLIDABLE) != nullptr);
    setRideFlag(owner, mob, ActorFlag::WasdControlled, ground);
    setRideFlag(owner, mob, ActorFlag::WasdFreeCameraControlled, air || water);
    setRideFlag(owner, mob, ActorFlag::DoesServerAuthOnlyDismount, air);
    setRideFlag(owner, mob, ActorFlag::CanUseVerticalMovementAction,
                air && mob.getComponent(VERTICAL_MOVEMENT_ACTION) != nullptr);
    setRideFlag(owner, mob, ActorFlag::BodyRotationAlwaysFollowsHead,
                air || water || mob.getComponent(BODY_ROTATION_ALWAYS_FOLLOWS_HEAD) != nullptr);
}

void RideControlSystem::reset(MobActor &mob) {
    RideControlState &state = mob.getRideControl();
    const int64_t dashCooldownEnd = state.mDashCooldownEndTick;
    const int32_t boostTicks = state.mBoostTicks;
    const float boostMultiplier = state.mBoostMultiplier;

    state = RideControlState();
    state.mDashCooldownEndTick = dashCooldownEnd;
    state.mBoostTicks = boostTicks;
    state.mBoostMultiplier = boostMultiplier;
}
