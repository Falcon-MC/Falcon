#include "World/Actor/AI/Goal/DragonGoals.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Level.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Core/Math/VectorMath.h"
#include "World/Level/Block/BlockNames.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {

    const char *const FLYING_GROUP = "dragon_flying";
    const char *const SITTING_GROUP = "dragon_sitting";
    const char *const DEATH_GROUP = "dragon_death";

    const char *const ON_START_LANDING = "minecraft:on_start_landing";
    const char *const ON_START_TAKEOFF = "minecraft:on_start_takeoff";

    const float MIN_TARGET_DISTANCE_SQUARED = 100.0f;
    const float MAX_TARGET_DISTANCE_SQUARED = 22500.0f;
    const float NODE_HEIGHT_SPREAD = 20.0f;

    const float LANDING_PLAYER_RANGE = 128.0f;
    const float HOLDING_PLAYER_RANGE = 64.0f;
    const int32_t HOLDING_NO_PLAYER_CHANCE = 66;
    const float SCAN_NEAR_RANGE = 20.0f;
    const float SCAN_FAR_RANGE = 150.0f;
    const float SCAN_FLAME_TIME = 25.0f;
    const float SCAN_GIVE_UP_TIME = 100.0f;
    const float SCAN_VIEW_ANGLE = 10.0f;
    const float TAKEOFF_LEAVE_DISTANCE_SQUARED = 100.0f;
    const float NODE_REACH = 40.0f;
    const float NODE_REACH_Y = 105.0f;
    const float LANDED_DISTANCE_SQUARED = 1.0f;
    const float LANDING_FLY_SPEED = 1.5f;
    const float DEFAULT_TURN_SPEED = 0.7f;
    const float DEATH_FLY_SPEED = 3.0f;
    const int32_t DEATH_PARTICLE_INTERVAL = 10;

    // the level event the game sends for a particle it adds, ored with the particle type
    const int32_t ADD_PARTICLE_EVENT = 0x4000;
    const int32_t HUGE_EXPLOSION_PARTICLE = 17;
    const int32_t DRAGON_FLAMING_EVENT = 68;

    const int32_t DRAGON_BREATH_POTION = 23;
    const int32_t DRAGON_BREATH_PARTICLE = 49;
    const float SMOKE_RADIUS_ON_USE = -0.5f;

    const float DEGREES_TO_RADIANS = 0.017453292f;
    const float RADIANS_TO_DEGREES = 57.295776f;

    using VectorMath::distanceSquared;

    using VectorMath::wrapDegrees;

    // the angle in degrees between the dragon's flight direction and the target, plus the game's half degree
    float angleTo(const EnderDragon &dragon, const Vector3f &target) {
        const Vector3f position = dragon.getPosition();
        const float dx = target.x - position.x;
        const float dz = target.z - position.z;
        const float length = std::sqrt(dx * dx + dz * dz);
        if (length < 0.0001f)
            return 0.5f;

        const float yaw = dragon.getRotation().y * DEGREES_TO_RADIANS;
        const float dot = std::sin(yaw) * (dx / length) - std::cos(yaw) * (dz / length);
        return std::acos(std::max(-1.0f, std::min(1.0f, dot))) * RADIANS_TO_DEGREES + 0.5f;
    }

    bool canAttack(const EnderDragon &dragon, const ServerPlayer *player) {
        return player != nullptr && dragon.canTarget(*player);
    }

    float numberOf(const json::Value &component, const char *key, float fallback) {
        const json::Value *value = component.get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }

    int32_t ticksOf(const json::Value &component, const char *key, int32_t fallback) {
        const json::Value *value = component.get(key);
        if (value == nullptr)
            return fallback;
        return (int32_t) ((float) value->number(0.0) * 20.0f);
    }

    void zoneOf(const json::Value &component, float &minimum, float &maximum) {
        const json::Value *zone = component.get("target_zone");
        if (zone == nullptr)
            return;

        if (zone->isArray() && zone->mArray.size() >= 2) {
            minimum = (float) zone->mArray[0]->number(minimum);
            maximum = (float) zone->mArray[1]->number(maximum);
            return;
        }

        minimum = numberOf(*zone, "min", minimum);
        maximum = numberOf(*zone, "max", maximum);
    }

}

DragonGoal::DragonGoal(const char *group) : mGroup(group) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool DragonGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (!mob.hasComponentGroup(mGroup))
        return false;

    return canStart(owner, dragonOf(mob));
}

EnderDragon &DragonGoal::dragonOf(MobActor &mob) {
    return static_cast<EnderDragon &>(mob);
}

void DragonGoal::navigate(EnderDragon &dragon, EnderDragon::Path *path) {
    if (path == nullptr || path->isDone())
        return;

    const Vector3i node = path->getNextNode();
    path->advance();

    float y;
    do {
        y = (float) node.y + dragon.nextFloat() * NODE_HEIGHT_SPREAD;
    } while ((float) node.y > y);

    dragon.setTargetLocation(Vector3f((float) node.x, y, (float) node.z));
}

int32_t DragonGoal::ringNode(int32_t node, int32_t crystals) {
    if (crystals <= 0)
        return ((node & 7) ^ 4) + 12;

    const int32_t wrapped = node % 12;
    return wrapped < 0 ? wrapped + 12 : wrapped;
}

bool DragonGoal::needsNewTarget(const EnderDragon &dragon) {
    const std::optional<Vector3f> &target = dragon.getTargetLocation();
    if (!target.has_value())
        return true;

    const float distance = distanceSquared(dragon.getPosition(), *target);
    return distance < MIN_TARGET_DISTANCE_SQUARED || distance > MAX_TARGET_DISTANCE_SQUARED;
}

bool DragonGoal::hasTarget(ServerNetworkHandler &owner, EnderDragon &dragon) {
    return dragon.getTarget(owner) != nullptr;
}

DragonHoldingPatternGoal::DragonHoldingPatternGoal() : DragonGoal(FLYING_GROUP) {}

bool DragonHoldingPatternGoal::canStart(ServerNetworkHandler &owner, EnderDragon &dragon) {
    return !dragon.isSitting() && !hasTarget(owner, dragon);
}

bool DragonHoldingPatternGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    return !hasTarget(owner, dragonOf(mob));
}

void DragonHoldingPatternGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    EnderDragon &dragon = dragonOf(mob);
    dragon.setTargetLocation(std::nullopt);
    dragon.clearTarget();
    mPath.reset();
}

void DragonHoldingPatternGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    dragonOf(mob).setTargetLocation(std::nullopt);
    mPath.reset();
}

void DragonHoldingPatternGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    EnderDragon &dragon = dragonOf(mob);
    if (needsNewTarget(dragon))
        _findNewTarget(owner, dragon);
}

void DragonHoldingPatternGoal::_findNewTarget(ServerNetworkHandler &owner, EnderDragon &dragon) {
    const int32_t crystals = dragon.getCrystalsAlive();

    if (mPath != nullptr && mPath->isDone()) {
        Level &level = owner.getLevelFor(dragon);
        const int32_t portalY = EnderDragon::getPortalY(level);

        if (dragon.nextInt(crystals + 3) == 0) {
            dragon.fireTrigger(owner, ON_START_LANDING);
            return;
        }

        ServerPlayer *player = EnderDragon::nearestPlayer(owner, level, Vector3f(0.0f, (float) portalY, 0.0f),
                                                          HOLDING_PLAYER_RANGE);
        if (player != nullptr) {
            const Vector3f other = player->getPosition();
            const Vector3f portal(0.5f, (float) portalY + 0.5f, 0.5f);
            const int32_t chance = std::abs((int32_t) (distanceSquared(portal, other) * 0.001953125f)) + 2;
            if ((dragon.nextInt(chance) == 0 || dragon.nextInt(crystals + 2) == 0) && canAttack(dragon, player)) {
                dragon.setTarget(owner, player->getRuntimeId());
                return;
            }
        } else {
            dragon.nextInt(HOLDING_NO_PLAYER_CHANCE);
        }
    }

    if (mPath == nullptr || mPath->isDone()) {
        const int32_t from = dragon.findClosestNode(owner);
        int32_t to = from;
        if (dragon.nextInt(8) == 0) {
            mClockwise = !mClockwise;
            to = from + 6;
        }

        to += mClockwise ? 1 : -1;
        mPath = dragon.findPath(from, ringNode(to, crystals), std::nullopt);
        if (mPath != nullptr)
            mPath->advance();
    }

    navigate(dragon, mPath.get());
}

DragonStrafePlayerGoal::DragonStrafePlayerGoal(const Settings &settings)
        : DragonGoal(FLYING_GROUP), mSettings(settings) {}

bool DragonStrafePlayerGoal::canStart(ServerNetworkHandler &owner, EnderDragon &dragon) {
    return !dragon.isSitting() && hasTarget(owner, dragon);
}

bool DragonStrafePlayerGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) mob;
    if (mFired)
        return false;

    Actor *target = MobActor::findActor(owner, mTarget);
    return target != nullptr && target->isAlive() && !target->isDead();
}

void DragonStrafePlayerGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    EnderDragon &dragon = dragonOf(mob);
    mFired = false;
    mInView = 0;
    mPath.reset();
    mTarget = 0;

    Actor *target = dragon.getTarget(owner);
    if (target != nullptr) {
        mTarget = target->getRuntimeId();
        const Vector3f aim = target->getPosition();
        const Vector3f position = dragon.getPosition();
        const int32_t from = dragon.findClosestNode(owner);
        const int32_t to = dragon.findClosestNode(owner, aim.x, aim.y, aim.z);

        const float floorX = std::floor(aim.x);
        const float floorZ = std::floor(aim.z);
        const float horizontal = std::sqrt((floorX - position.x) * (floorX - position.x)
                                           + (floorZ - position.z) * (floorZ - position.z));
        const float lift = std::min(10.0f, horizontal / 80.0f + 0.4f - 1.0f) + aim.y;
        const Vector3i finalNode((int32_t) floorX, (int32_t) std::floor(lift), (int32_t) floorZ);

        mPath = dragon.findPath(from, to, finalNode);
        if (mPath != nullptr)
            mPath->advance();
        navigate(dragon, mPath.get());
    }

    dragon.setSitting(owner, false);
    dragon.setFlySpeed(mSettings.mActiveSpeed);
    dragon.setTurnSpeed(mSettings.mTurnSpeed);
}

void DragonStrafePlayerGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    EnderDragon &dragon = dragonOf(mob);
    dragon.clearTarget();
    dragon.setTargetLocation(std::nullopt);
    dragon.setFlySpeed(mSettings.mFlightSpeed);
    dragon.setTurnSpeed(mSettings.mTurnSpeed);
}

void DragonStrafePlayerGoal::_findNewTarget(ServerNetworkHandler &owner, EnderDragon &dragon) {
    const int32_t from = dragon.findClosestNode(owner);
    int32_t to = from;
    if (mSettings.mSwitchDirectionProbability != 0.0f && mSettings.mSwitchDirectionProbability >= dragon.nextFloat()) {
        mClockwise = !mClockwise;
        to = from + 6;
    }

    to += mClockwise ? 1 : -1;
    mPath = dragon.findPath(from, ringNode(to, dragon.getCrystalsAlive()), std::nullopt);
    if (mPath != nullptr)
        mPath->advance();
}

void DragonStrafePlayerGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    EnderDragon &dragon = dragonOf(mob);
    Actor *target = MobActor::findActor(owner, mTarget);
    if (target == nullptr || target->isDead())
        return;

    const Vector3f aim = target->getPosition();
    const Vector3f position = dragon.getPosition();
    if (mPath != nullptr && mPath->isDone()) {
        const float dx = aim.x - position.x;
        const float dz = aim.z - position.z;
        const float lift = std::min(10.0f, std::sqrt(dx * dx + dz * dz) / 80.0f + 0.4f - 1.0f);
        dragon.setTargetLocation(Vector3f(aim.x, aim.y + lift, aim.z));
    }

    const std::optional<Vector3f> &location = dragon.getTargetLocation();
    const float distance = location.has_value() ? distanceSquared(position, *location) : 0.0f;
    if (mSettings.mMinDistance * mSettings.mMinDistance > distance
        || distance > mSettings.mMaxDistance * mSettings.mMaxDistance) {
        if (mPath == nullptr || mPath->isDone())
            _findNewTarget(owner, dragon);
        navigate(dragon, mPath.get());
    }

    ServerPlayer *player = dynamic_cast<ServerPlayer *>(target);
    const bool inRange = mSettings.mFireballRange * mSettings.mFireballRange > distanceSquared(aim, position);
    if (!inRange || !canAttack(dragon, player)) {
        if (mInView > 0)
            mInView--;
        return;
    }

    mInView++;
    if (mInView < mSettings.mInViewTicks || mFired)
        return;

    const float angle = angleTo(dragon, aim);
    if (angle < 0.0f || mSettings.mViewAngle <= angle)
        return;

    dragon.shootFireball(owner, *target);
    mInView = 0;
    if (mPath != nullptr)
        mPath->finish();
    mFired = true;
}

DragonChargePlayerGoal::DragonChargePlayerGoal(const Settings &settings)
        : DragonGoal(FLYING_GROUP), mSettings(settings) {}

bool DragonChargePlayerGoal::canStart(ServerNetworkHandler &owner, EnderDragon &dragon) {
    return dragon.isSitting() && hasTarget(owner, dragon);
}

bool DragonChargePlayerGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    if (!dragonOf(mob).getTargetLocation().has_value())
        return false;

    if (mTimer <= 0)
        return true;

    return mTimer++ < mSettings.mContinueTicks;
}

void DragonChargePlayerGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    EnderDragon &dragon = dragonOf(mob);
    mTimer = 0;
    dragon.setSitting(owner, false);
    dragon.setFlySpeed(mSettings.mActiveSpeed);
    dragon.setTurnSpeed(mSettings.mTurnSpeed);

    ServerPlayer *player = EnderDragon::nearestPlayer(owner, owner.getLevelFor(dragon), dragon.getPosition(),
                                                      mSettings.mMaxDistance);
    if (player != nullptr)
        dragon.setTargetLocation(player->getPosition());
    else
        dragon.setTargetLocation(std::nullopt);
}

void DragonChargePlayerGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    EnderDragon &dragon = dragonOf(mob);
    mTimer = 0;
    dragon.setTargetLocation(std::nullopt);
    dragon.setFlySpeed(mSettings.mFlightSpeed);
    dragon.clearTarget();
}

void DragonChargePlayerGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    EnderDragon &dragon = dragonOf(mob);
    const std::optional<Vector3f> &location = dragon.getTargetLocation();
    if (!location.has_value())
        return;

    const float distance = distanceSquared(dragon.getPosition(), *location);
    if (mSettings.mMinDistance * mSettings.mMinDistance > distance
        || distance > mSettings.mMaxDistance * mSettings.mMaxDistance)
        mTimer++;
}

DragonLandingGoal::DragonLandingGoal() : DragonGoal(SITTING_GROUP) {}

bool DragonLandingGoal::canStart(ServerNetworkHandler &owner, EnderDragon &dragon) {
    (void) owner;
    return !dragon.isSitting();
}

bool DragonLandingGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    EnderDragon &dragon = dragonOf(mob);
    if (!mFinalApproach || !dragon.getTargetLocation().has_value())
        return true;

    if (distanceSquared(dragon.getPosition(), *dragon.getTargetLocation()) >= LANDED_DISTANCE_SQUARED)
        return true;

    dragon.resetFlameCount();
    dragon.setSitting(owner, true);
    return false;
}

void DragonLandingGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    EnderDragon &dragon = dragonOf(mob);
    dragon.setTargetLocation(std::nullopt);
    mPath.reset();
    dragon.setFlySpeed(EnderDragon::DEFAULT_FLY_SPEED);
    dragon.setTurnSpeed(DEFAULT_TURN_SPEED);
    mFinalApproach = false;
}

void DragonLandingGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    EnderDragon &dragon = dragonOf(mob);
    dragon.setTargetLocation(std::nullopt);
    dragon.setFlySpeed(EnderDragon::DEFAULT_FLY_SPEED);
    dragon.setTurnSpeed(DEFAULT_TURN_SPEED);
    mFinalApproach = false;
}

void DragonLandingGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (mFinalApproach)
        return;

    EnderDragon &dragon = dragonOf(mob);
    if (!needsNewTarget(dragon))
        return;

    Level &level = owner.getLevelFor(dragon);
    const int32_t portalY = EnderDragon::getPortalY(level);

    if (mPath == nullptr || mPath->isDone()) {
        const int32_t from = dragon.findClosestNode(owner);
        ServerPlayer *player = EnderDragon::nearestPlayer(owner, level, Vector3f(0.0f, (float) portalY, 0.0f),
                                                          LANDING_PLAYER_RANGE);
        int32_t to;
        if (player != nullptr) {
            const Vector3f other = player->getPosition();
            const float length = std::sqrt(other.x * other.x + other.z * other.z);
            const float x = length >= 0.0001f ? std::floor(other.x / length * -NODE_REACH) : -0.0f;
            const float z = length >= 0.0001f ? std::floor(other.z / length * -NODE_REACH) : -0.0f;
            to = dragon.findClosestNode(owner, x, NODE_REACH_Y, z);
        } else {
            to = dragon.findClosestNode(owner, NODE_REACH, (float) portalY, 0.0f);
        }

        mPath = dragon.findPath(from, to, Vector3i(0, portalY, 0));
        if (mPath != nullptr)
            mPath->advance();
    }

    navigate(dragon, mPath.get());

    if (mPath != nullptr && mPath->isDone()) {
        mFinalApproach = true;
        dragon.setTargetLocation(Vector3f(0.0f, (float) portalY, 0.0f));
        dragon.setFlySpeed(LANDING_FLY_SPEED);
        dragon.setTurnSpeed(0.0f);
    }
}

DragonFlamingGoal::DragonFlamingGoal(const Settings &settings) : DragonGoal(SITTING_GROUP), mSettings(settings) {}

bool DragonFlamingGoal::canStart(ServerNetworkHandler &owner, EnderDragon &dragon) {
    return hasTarget(owner, dragon);
}

bool DragonFlamingGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    EnderDragon &dragon = dragonOf(mob);
    if (mTicks >= mSettings.mCooldownTicks && dragon.getFlameCount() >= mSettings.mGroundFlameCount)
        dragon.fireTrigger(owner, ON_START_TAKEOFF);

    return mTicks < mSettings.mCooldownTicks;
}

void DragonFlamingGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    EnderDragon &dragon = dragonOf(mob);
    mRoar = 0;
    dragon.setTargetLocation(std::nullopt);
    dragon.setSitting(owner, true);
    dragon.addFlame();
}

void DragonFlamingGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    EnderDragon &dragon = dragonOf(mob);
    mTicks = 0;
    mRoar = 0;
    dragon.setTargetLocation(std::nullopt);
    dragon.clearTarget();
}

void DragonFlamingGoal::_breathe(ServerNetworkHandler &owner, EnderDragon &dragon) {
    Level &level = owner.getLevelFor(dragon);
    const Vector3f head = dragon.getPartCenter(EnderDragon::Part::Head);
    const Vector3f position = dragon.getPosition();
    const float headX = std::floor(head.x);
    const float headZ = std::floor(head.z);

    float dirX = headX - position.x;
    float dirZ = headZ - position.z;
    const float length = std::sqrt(dirX * dirX + dirZ * dirZ);
    dirX = length < 0.0001f ? 0.0f : dirX / length;
    dirZ = length < 0.0001f ? 0.0f : dirZ / length;

    const float x = headX + dirX * mSettings.mSmokeRadius * 0.5f;
    const float z = headZ + dirZ * mSettings.mSmokeRadius * 0.5f;
    const int32_t blockX = (int32_t) std::floor(x);
    const int32_t blockZ = (int32_t) std::floor(z);

    float y = std::floor(head.y);
    const BlockState *state = level.peekBlockPtr(blockX, (int32_t) y, blockZ);
    if (state == nullptr || state->mName == BlockNames::AIR) {
        while (true) {
            y -= 1.0f;
            if ((int32_t) std::floor(y) < level.getMinY())
                break;

            state = level.peekBlockPtr(blockX, (int32_t) std::floor(y), blockZ);
            if (state != nullptr && state->mName != BlockNames::AIR)
                break;
        }
    }

    ServerNetworkHandler::AreaEffectCloudSettings settings;
    settings.mPotionId = DRAGON_BREATH_POTION;
    settings.mRadius = mSettings.mSmokeRadius;
    settings.mRadiusOnUse = SMOKE_RADIUS_ON_USE;
    settings.mDuration = mSettings.mSmokeTicks;
    settings.mParticle = DRAGON_BREATH_PARTICLE;
    settings.mColor = mSettings.mSmokeColor;
    settings.mOwnerRuntimeId = (int64_t) dragon.getRuntimeId();
    owner.spawnAreaEffectCloud(level, Vector3f(x, std::floor(y) + 1.0f, z), settings);
}

void DragonFlamingGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    EnderDragon &dragon = dragonOf(mob);
    if (mRoar++ < mSettings.mRoarTicks) {
        owner.playLevelSound(owner.getLevelFor(dragon), "mad", dragon.getPosition(), EnderDragon::IDENTIFIER);
        return;
    }

    const int32_t previous = mTicks++;
    if ((previous & 1) != 0 && previous + 1 < mSettings.mFlameTicks)
        owner.broadcastActorEvent(dragon, (EntityEventType) DRAGON_FLAMING_EVENT);

    if (mTicks == mSettings.mFlameTicks)
        _breathe(owner, dragon);
}

DragonScanningGoal::DragonScanningGoal() : DragonGoal(SITTING_GROUP) {}

bool DragonScanningGoal::canStart(ServerNetworkHandler &owner, EnderDragon &dragon) {
    return dragon.isSitting() && !hasTarget(owner, dragon);
}

bool DragonScanningGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    EnderDragon &dragon = dragonOf(mob);
    Level &level = owner.getLevelFor(dragon);
    const Vector3f position(std::floor(dragon.getPosition().x), std::floor(dragon.getPosition().y),
                            std::floor(dragon.getPosition().z));

    ServerPlayer *nearPlayer = EnderDragon::nearestPlayer(owner, level, position, SCAN_NEAR_RANGE);
    if (nearPlayer != nullptr) {
        if (mScanTime <= SCAN_FLAME_TIME)
            return true;

        dragon.setTarget(owner, nearPlayer->getRuntimeId());
        return false;
    }

    if (mScanTime < SCAN_GIVE_UP_TIME)
        return true;

    ServerPlayer *farPlayer = EnderDragon::nearestPlayer(owner, level, position, SCAN_FAR_RANGE);
    if (canAttack(dragon, farPlayer))
        dragon.setTarget(owner, farPlayer->getRuntimeId());

    dragon.fireTrigger(owner, ON_START_TAKEOFF);
    return false;
}

void DragonScanningGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    EnderDragon &dragon = dragonOf(mob);
    mScanTime = 0.0f;
    dragon.setTargetLocation(std::nullopt);
    dragon.clearTarget();
}

void DragonScanningGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    dragonOf(mob).setTargetLocation(std::nullopt);
}

void DragonScanningGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    EnderDragon &dragon = dragonOf(mob);
    mScanTime += 1.0f;

    const Vector3f position(std::floor(dragon.getPosition().x), std::floor(dragon.getPosition().y),
                            std::floor(dragon.getPosition().z));
    ServerPlayer *player = EnderDragon::nearestPlayer(owner, owner.getLevelFor(dragon), position, SCAN_NEAR_RANGE);
    if (player == nullptr)
        return;

    const Vector3f other = player->getPosition();
    const float angle = angleTo(dragon, other);
    if (angle >= 0.0f && angle <= SCAN_VIEW_ANGLE)
        return;

    // sub_14D1D2130 turns the head toward the player, faster the closer it is
    const Vector3f head = dragon.getPartCenter(EnderDragon::Part::Head);
    const float dx = other.x - std::floor(head.x);
    const float dz = other.z - std::floor(head.z);
    Vector3f rotation = dragon.getRotation();
    const float turn = std::max(
            -100.0f, std::min(100.0f, wrapDegrees(180.0f - std::atan2(dx, dz) * RADIANS_TO_DEGREES - rotation.y)));
    const float reach = std::sqrt(dx * dx + dz * dz) + 1.0f;
    const float acceleration = 1.0f / std::min(40.0f, reach) / reach * turn + dragon.getYawAcceleration() * 0.8f;
    dragon.setYawAcceleration(acceleration);
    rotation.y += acceleration;
    rotation.z = rotation.y;
    dragon.setRotation(rotation);
}

DragonTakeoffGoal::DragonTakeoffGoal() : DragonGoal(FLYING_GROUP) {}

bool DragonTakeoffGoal::canStart(ServerNetworkHandler &owner, EnderDragon &dragon) {
    return dragon.isSitting() && !hasTarget(owner, dragon);
}

bool DragonTakeoffGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    EnderDragon &dragon = dragonOf(mob);
    const int32_t portalY = EnderDragon::getPortalY(owner.getLevelFor(dragon));
    const Vector3f portal(0.5f, (float) portalY + 0.5f, 0.5f);
    return distanceSquared(portal, dragon.getPosition()) <= TAKEOFF_LEAVE_DISTANCE_SQUARED;
}

void DragonTakeoffGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    EnderDragon &dragon = dragonOf(mob);
    dragon.setTargetLocation(std::nullopt);
    mPath.reset();
    dragon.setSitting(owner, false);
    mFirstTick = true;
}

void DragonTakeoffGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    dragonOf(mob).setTargetLocation(std::nullopt);
    mPath.reset();
}

void DragonTakeoffGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (!mFirstTick)
        return;

    mFirstTick = false;
    EnderDragon &dragon = dragonOf(mob);
    const int32_t from = dragon.findClosestNode(owner);

    const Vector3f rotation = dragon.getRotation();
    const float yaw = rotation.y * DEGREES_TO_RADIANS;
    const float pitch = rotation.x * DEGREES_TO_RADIANS;
    const float viewX = -std::sin(yaw) * std::cos(pitch);
    const float viewZ = std::cos(yaw) * std::cos(pitch);
    const int32_t ahead = dragon.findClosestNode(owner, std::floor(viewX * -NODE_REACH), NODE_REACH_Y,
                                                 std::floor(-NODE_REACH * viewZ));

    mPath = dragon.findPath(from, ringNode(ahead, dragon.getCrystalsAlive()), std::nullopt);
    navigate(dragon, mPath.get());
}

DragonDeathGoal::DragonDeathGoal() : DragonGoal(DEATH_GROUP) {}

bool DragonDeathGoal::canStart(ServerNetworkHandler &owner, EnderDragon &dragon) {
    (void) owner;
    return dragon.getHealth() > 0.0f;
}

bool DragonDeathGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    return mob.getHealth() > 0.0f;
}

void DragonDeathGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    EnderDragon &dragon = dragonOf(mob);
    dragon.setTargetLocation(std::nullopt);
    dragon.setFlySpeed(DEATH_FLY_SPEED);
    dragon.setTurnSpeed(DEFAULT_TURN_SPEED);
    mTicks = 0;
}

void DragonDeathGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    EnderDragon &dragon = dragonOf(mob);
    dragon.setTargetLocation(std::nullopt);
    dragon.setFlySpeed(EnderDragon::DEFAULT_FLY_SPEED);
}

void DragonDeathGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    EnderDragon &dragon = dragonOf(mob);
    Level &level = owner.getLevelFor(dragon);

    if (mTicks++ % DEATH_PARTICLE_INTERVAL == DEATH_PARTICLE_INTERVAL - 1) {
        const float x = (dragon.nextFloat() - 0.5f) * 8.0f;
        const float y = (dragon.nextFloat() - 0.5f) * 4.0f + 2.0f;
        const float z = (dragon.nextFloat() - 0.5f) * 8.0f;
        const Vector3f position = dragon.getPosition();
        owner.broadcastLevelEvent(level, ADD_PARTICLE_EVENT | HUGE_EXPLOSION_PARTICLE,
                                  Vector3f(position.x + x, position.y + y, position.z + z), 0);
    }

    if (!dragon.getTargetLocation().has_value())
        dragon.setTargetLocation(Vector3f(0.0f, (float) EnderDragon::getPortalY(level), 0.0f));

    const float distance = distanceSquared(dragon.getPosition(), *dragon.getTargetLocation());
    if (distance < MIN_TARGET_DISTANCE_SQUARED || distance > MAX_TARGET_DISTANCE_SQUARED) {
        dragon.finishDying(owner);
        return;
    }

    dragon.setHealth(1.0f);
}

std::unique_ptr<Goal> DragonGoals::create(const MobActor &mob, const std::string &behavior,
                                          const json::Value &component) {
    if (dynamic_cast<const EnderDragon *>(&mob) == nullptr)
        return nullptr;

    if (behavior == "dragonholdingpattern")
        return std::make_unique<DragonHoldingPatternGoal>();

    if (behavior == "dragonstrafeplayer") {
        DragonStrafePlayerGoal::Settings settings;
        settings.mActiveSpeed = numberOf(component, "active_speed", settings.mActiveSpeed);
        settings.mFlightSpeed = numberOf(component, "flight_speed", settings.mFlightSpeed);
        settings.mTurnSpeed = numberOf(component, "turn_speed", settings.mTurnSpeed);
        zoneOf(component, settings.mMinDistance, settings.mMaxDistance);
        settings.mInViewTicks = ticksOf(component, "target_in_range_and_in_view_time", settings.mInViewTicks);
        settings.mFireballRange = numberOf(component, "fireball_range", settings.mFireballRange);
        settings.mSwitchDirectionProbability = numberOf(component, "switch_direction_probability",
                                                        settings.mSwitchDirectionProbability);
        settings.mViewAngle = numberOf(component, "view_angle", settings.mViewAngle);
        return std::make_unique<DragonStrafePlayerGoal>(settings);
    }

    if (behavior == "dragonchargeplayer") {
        DragonChargePlayerGoal::Settings settings;
        settings.mActiveSpeed = numberOf(component, "active_speed", settings.mActiveSpeed);
        settings.mFlightSpeed = numberOf(component, "flight_speed", settings.mFlightSpeed);
        settings.mTurnSpeed = numberOf(component, "turn_speed", settings.mTurnSpeed);
        zoneOf(component, settings.mMinDistance, settings.mMaxDistance);
        settings.mContinueTicks = ticksOf(component, "continue_charge_threshold_time", settings.mContinueTicks);
        return std::make_unique<DragonChargePlayerGoal>(settings);
    }

    if (behavior == "dragonlanding")
        return std::make_unique<DragonLandingGoal>();

    if (behavior == "dragonflaming") {
        DragonFlamingGoal::Settings settings;
        const json::Value *count = component.get("ground_flame_count");
        if (count != nullptr)
            settings.mGroundFlameCount = (int32_t) count->number(settings.mGroundFlameCount);
        settings.mCooldownTicks = ticksOf(component, "cooldown_time", settings.mCooldownTicks);
        settings.mFlameTicks = ticksOf(component, "flame_time", settings.mFlameTicks);
        settings.mRoarTicks = ticksOf(component, "roar_time", settings.mRoarTicks);
        settings.mSmokeTicks = std::max(0, ticksOf(component, "smoke_time", settings.mSmokeTicks));
        settings.mSmokeRadius = numberOf(component, "smoke_radius", settings.mSmokeRadius);
        return std::make_unique<DragonFlamingGoal>(settings);
    }

    if (behavior == "dragonscanning")
        return std::make_unique<DragonScanningGoal>();

    if (behavior == "dragontakeoff")
        return std::make_unique<DragonTakeoffGoal>();

    if (behavior == "dragondeath")
        return std::make_unique<DragonDeathGoal>();

    return nullptr;
}
