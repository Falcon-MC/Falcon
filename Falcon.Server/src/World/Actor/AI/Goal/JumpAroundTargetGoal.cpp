#include "World/Actor/AI/Goal/JumpAroundTargetGoal.h"

#include "Core/Math/AxisAlignedBB.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/AI/Control/LookControl.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "World/Actor/Movement/MovementPhysics.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockSolidity.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Explosion.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
    const char *const JUMP_SOUND = "jump";
    const char *const AIR = "minecraft:air";
    const char *const LAVA = "minecraft:lava";
    const char *const FLOWING_LAVA = "minecraft:flowing_lava";
    const float DEGREES_TO_RADIANS = 0.017453292f;
    const float PI = 3.1415927f;
    // the gravity the arc is worked out with
    const float ARC_GRAVITY = 0.08f;
    // the arc is checked in steps of this part of the box's smallest side
    const float ARC_STEP_RATIO = 0.9f;
    // the block a mob stands on is looked for this far under its box
    const float STANDING_PROBE = 0.2f;
    const double SIGHT_STEP = 0.25;
    const float SIGHT_EYE_RATIO = 0.85f;
    // a landing in water only counts after these ticks
    const int32_t WATER_LANDING_DELAY = 40;

    const BlockState *blockAt(Level &level, int32_t x, int32_t y, int32_t z) {
        return level.peekBlockPtr(x, y, z);
    }

    bool isSolidAt(Level &level, int32_t x, int32_t y, int32_t z) {
        const BlockState *state = blockAt(level, x, y, z);
        return state != nullptr && BlockSolidity::isSolid(state->mName);
    }

    bool isAirAt(Level &level, int32_t x, int32_t y, int32_t z) {
        const BlockState *state = blockAt(level, x, y, z);
        return state != nullptr && state->mName == AIR;
    }

    // the game's block under a mob: the top of the highest box under it, else the block of its feet
    Vector3i standingBlock(Level &level, const MobActor &mob) {
        const AxisAlignedBB box = ActorPushSystem::boundingBoxOf(mob);
        const AxisAlignedBB probe(box.mMinX, box.mMinY - STANDING_PROBE, box.mMinZ, box.mMaxX, box.mMinY, box.mMaxZ);
        const Vector3f position = mob.getPosition();
        Vector3f found = position;
        float highest = -1.0e30f;
        for (const AxisAlignedBB &obstacle: level.getCollisionBoxes(probe, &mob)) {
            if (obstacle.mMaxY > highest) {
                highest = obstacle.mMaxY;
                found = Vector3f(obstacle.mMinX, obstacle.mMinY, obstacle.mMinZ);
            }
        }
        return Vector3i((int32_t) std::floor(found.x), (int32_t) std::floor(found.y), (int32_t) std::floor(found.z));
    }

    bool sees(Level &level, const MobActor &mob, const Vector3f &point) {
        const Vector3f position = mob.getPosition();
        return !Explosion::isRayCollidingWithBlocks(level, position.x,
                                                    position.y + mob.getSize().mHeight * SIGHT_EYE_RATIO, position.z,
                                                    point.x, point.y, point.z, SIGHT_STEP);
    }

    void setJumping(ServerNetworkHandler &owner, MobActor &mob, bool jumping) {
        if (mob.getFlags().get(ActorFlag::JumpGoalJump) == jumping)
            return;
        mob.getFlags().set(ActorFlag::JumpGoalJump, jumping);
        owner.syncActorFlags(mob);
    }

    bool inWater(Level &level, const MobActor &mob) {
        return LiquidBlocksFetch::at(level, mob.getPosition()).water;
    }
}

JumpAroundTargetGoal::JumpAroundTargetGoal(Settings settings)
        : mSettings(std::move(settings)), mRandom(std::random_device{}()) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

Vector3f JumpAroundTargetGoal::positionAroundTarget(const Actor &target, float spreadDegrees, float minDistance,
                                                    float maxDistance, std::mt19937 &random) {
    // behind the way the head faces, spread by a gaussian
    const float headYaw = target.getRotation().z + 180.0f;
    std::normal_distribution<float> gaussian(0.0f, 1.0f);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const float spread = gaussian(random);
    const float distance = (maxDistance - minDistance) * unit(random) + minDistance;
    const float angle = (headYaw + spreadDegrees * spread * 0.5f) * -DEGREES_TO_RADIANS - PI;
    const Vector3f feet = target.getPosition();
    return Vector3f(feet.x - MovementPhysics::mojangSin(angle) * distance, feet.y,
                    feet.z - MovementPhysics::mojangCos(angle) * distance);
}

std::optional<Vector3f> JumpAroundTargetGoal::launchVelocity(Level &level, const AxisAlignedBB &box,
                                                             const Vector3f &from, const Vector3f &to,
                                                             float angleDegrees, float maxVelocity, float boxScale,
                                                             bool checkCollision) {
    const float dx = to.x - from.x;
    const float dz = to.z - from.z;
    const float horizontalSquared = dx * dx + dz * dz;
    const float horizontal = std::sqrt(horizontalSquared);
    const float sine = MovementPhysics::mojangSin(angleDegrees * DEGREES_TO_RADIANS);
    const float cosine = MovementPhysics::mojangCos(angleDegrees * DEGREES_TO_RADIANS);
    const float rise = to.y - from.y;
    const float speedSquared = std::fabs(horizontalSquared * ARC_GRAVITY
                                         / (2.0f * sine * cosine * horizontal - 2.0f * rise * cosine * cosine));
    const float speed = std::sqrt(speedSquared);
    if (!(speed <= maxVelocity))
        return std::nullopt;

    const float horizontalSpeed = cosine * speed;
    const float yaw = std::atan2(dz, dx);
    const float yawSine = MovementPhysics::mojangSin(yaw);
    const float yawCosine = MovementPhysics::mojangCos(yaw);
    const Vector3f velocity(horizontalSpeed * yawCosine, sine * speed, horizontalSpeed * yawSine);
    if (!checkCollision)
        return velocity;

    // points along the arc, twice as many as the ticks it takes; the shrunk box must clear the way between them
    const int32_t ticks = (int32_t) std::ceil(horizontal / horizontalSpeed);
    if (ticks <= 0)
        return std::nullopt;
    const float step = horizontal / (float) (2 * ticks);
    const int32_t points = std::max(1, 2 * ticks - 1);
    const float halfWidth = (box.mMaxX - box.mMinX) * boxScale * 0.5f;
    const float halfDepth = (box.mMaxZ - box.mMinZ) * boxScale * 0.5f;
    const float halfHeight = (box.mMaxY - box.mMinY) * boxScale * 0.5f;
    const float smallest = std::min({halfWidth, halfDepth, halfHeight}) * 2.0f;
    std::optional<Vector3f> previous;
    float travelled = 0.0f;
    for (int32_t point = 0; point < points; ++point) {
        travelled += step;
        const float height = travelled * sine / cosine
                             - travelled * travelled * ARC_GRAVITY / (2.0f * cosine * cosine * speedSquared);
        const Vector3f at(from.x + yawCosine * travelled, from.y + height, from.z + yawSine * travelled);
        if (previous.has_value() && boxScale != 0.0f && smallest > 0.0f) {
            const Vector3f segment(at.x - previous->x, at.y - previous->y, at.z - previous->z);
            const float length = std::sqrt(segment.x * segment.x + segment.y * segment.y + segment.z * segment.z);
            const float stride = smallest * ARC_STEP_RATIO;
            const int32_t strides = (int32_t) std::ceil(length / stride);
            const Vector3f advance = length < 0.0001f ? Vector3f()
                                                      : Vector3f(segment.x / length * stride, segment.y / length * stride,
                                                                 segment.z / length * stride);
            Vector3f centre = *previous;
            for (int32_t i = 0; i < strides; ++i) {
                centre = Vector3f(centre.x + advance.x, centre.y + advance.y, centre.z + advance.z);
                const AxisAlignedBB swept(centre.x - halfWidth, centre.y - halfHeight, centre.z - halfDepth,
                                          centre.x + halfWidth, centre.y + halfHeight, centre.z + halfDepth);
                if (!level.getCollisionBoxes(swept).empty())
                    return std::nullopt;
            }
        }
        previous = at;
    }
    return velocity;
}

std::optional<JumpAroundTargetGoal::Jump> JumpAroundTargetGoal::_plan(ServerNetworkHandler &owner, MobActor &mob,
                                                                       const Vector3f &landing) {
    std::vector<float> angles = mSettings.mJumpAngles;
    for (size_t i = 1; i < angles.size(); ++i)
        std::swap(angles[i], angles[std::uniform_int_distribution<size_t>(0, i)(mRandom)]);

    // the middle of the landing block, half a block back toward the mob along x and z alike
    const Vector3f position = mob.getPosition();
    const Vector3f middle(landing.x + 0.5f, landing.y + 0.5f, landing.z + 0.5f);
    const float dx = middle.x - position.x;
    const float length = std::sqrt(dx * dx + dx * dx);
    const float back = length < 0.0001f ? 0.0f : dx / length * 0.5f;
    const Vector3f aim(middle.x - back, middle.y, middle.z - back);

    Level &level = owner.getLevelFor(mob);
    const AxisAlignedBB box = ActorPushSystem::boundingBoxOf(mob);
    for (const float angle: angles) {
        std::optional<Vector3f> velocity = launchVelocity(level, box, position, aim, angle, mSettings.mMaxJumpVelocity,
                                                          mSettings.mBoxScale, mSettings.mCheckCollision);
        if (!velocity.has_value())
            continue;
        // jump boost lifts the launch along its own direction
        if (const MobEffectInstance *boost = mob.getEffect(MobEffectId::JumpBoost)) {
            const float speed = std::sqrt(velocity->x * velocity->x + velocity->y * velocity->y
                                          + velocity->z * velocity->z);
            const float upward = speed < 0.0001f ? 0.0f : velocity->y / speed;
            velocity->y += MovementPhysics::JUMP_BOOST_PER_LEVEL * (float) boost->level() * upward;
        }
        return Jump{landing, *velocity};
    }
    return std::nullopt;
}

bool JumpAroundTargetGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (owner.getCurrentTick() < mCooldownEnd)
        return false;
    if (mSettings.mFilters != nullptr && !EntityFilter::test(*mSettings.mFilters, owner, mob, &mob))
        return false;
    const Actor *target = mob.getTarget(owner);
    if (target == nullptr)
        return false;

    const Vector3f position = mob.getPosition();
    const Vector3f targetPosition = target->getPosition();
    const float distance = (position.x - targetPosition.x) * (position.x - targetPosition.x)
                           + (position.y - targetPosition.y) * (position.y - targetPosition.y)
                           + (position.z - targetPosition.z) * (position.z - targetPosition.z);
    const float minimum = mSettings.mMinTargetDistance;
    const float maximum = mSettings.mMaxTargetDistance;
    if (!(minimum * minimum < distance && distance < maximum * maximum))
        return false;

    Level &level = owner.getLevelFor(mob);
    // no jump off a block that cuts jumps, as honey does
    const Vector3i standing = standingBlock(level, mob);
    const BlockState *ground = blockAt(level, standing.x, standing.y, standing.z);
    const Block *groundBlock = ground == nullptr ? nullptr : VanillaBlocks::fromIdentifier(ground->mName);
    if (groundBlock != nullptr && groundBlock->getJumpFactor() != 1.0f)
        return false;

    const int32_t x = (int32_t) std::floor(position.x);
    const int32_t y = (int32_t) std::floor(position.y);
    const int32_t z = (int32_t) std::floor(position.z);
    for (int32_t above = 0; above < mSettings.mRequiredVerticalSpace; ++above) {
        if (!isAirAt(level, x, y + 1 + above, z))
            return false;
    }

    const Vector3f around = positionAroundTarget(*target, mSettings.mLandingSpreadDegrees,
                                                 mSettings.mMinLandingDistance, mSettings.mMaxLandingDistance, mRandom);
    // the surface within the snap range: down first, then up, at each distance
    const int32_t lx = (int32_t) std::floor(around.x);
    const int32_t ly = (int32_t) std::floor(around.y);
    const int32_t lz = (int32_t) std::floor(around.z);
    std::optional<Vector3i> surface;
    for (int32_t offset = 0; offset < mSettings.mSnapRange && !surface.has_value(); ++offset) {
        if (isSolidAt(level, lx, ly - offset, lz) && !isSolidAt(level, lx, ly - offset + 1, lz))
            surface = Vector3i(lx, ly - offset, lz);
        else if (isSolidAt(level, lx, ly + offset, lz) && !isSolidAt(level, lx, ly + offset + 1, lz))
            surface = Vector3i(lx, ly + offset, lz);
    }
    if (!surface.has_value())
        return false;
    const BlockState *landingBlock = blockAt(level, surface->x, surface->y, surface->z);
    if (landingBlock == nullptr || landingBlock->mName == LAVA || landingBlock->mName == FLOWING_LAVA)
        return false;

    // seen just above the spot, or as high as the obstruction it may look over
    const Vector3f low((float) surface->x, (float) (surface->y + 2), (float) surface->z);
    const Vector3f high((float) surface->x, (float) (surface->y + 1 + mSettings.mLineOfSightIgnoredHeight),
                        (float) surface->z);
    if (!sees(level, mob, low) && !sees(level, mob, high))
        return false;

    mJump = _plan(owner, mob, Vector3f((float) surface->x, (float) (surface->y + 1), (float) surface->z));
    return mJump.has_value();
}

bool JumpAroundTargetGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    return mPhase != Phase::Done;
}

void JumpAroundTargetGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mStartTick = owner.getCurrentTick();
    mHadDiscardFriction = mob.discardsFriction();
    setJumping(owner, mob, true);
    mPhase = Phase::Prepare;
    if (mJump.has_value()) {
        Vector3f rotation = mob.getRotation();
        rotation.y = LookControl::yawTowards(mob.getPosition(), mJump->mLanding);
        mob.setRotation(rotation);
    }
}

void JumpAroundTargetGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    setJumping(owner, mob, false);
    const int64_t now = owner.getCurrentTick();
    // a jump cut short by a recent hurt waits less
    const bool recentlyHurt = now - mob.getLastHurtTick() <= mSettings.mLastHurtDuration && mPhase == Phase::Done;
    mCooldownEnd = now + (recentlyHurt ? mSettings.mHurtCooldown : mSettings.mCooldown);
    mJump.reset();
    mob.setDiscardsFriction(mHadDiscardFriction);
    mob.setCollisionScale(1.0f);
}

void JumpAroundTargetGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const int64_t now = owner.getCurrentTick();
    Level &level = owner.getLevelFor(mob);
    if (mPhase == Phase::Prepare) {
        if (now - mStartTick < mSettings.mPrepareDuration)
            return;
        mPhase = Phase::Jumping;
        mWaterLandingTick = now + WATER_LANDING_DELAY;
        if (mJump.has_value()) {
            mob.setDiscardsFriction(true);
            mob.setCollisionScale(mSettings.mBoxScale);
            mob.setMotion(mJump->mVelocity);
            owner.playLevelSound(level, JUMP_SOUND, mob.getPosition(), mob.getIdentifier());
        }
        return;
    }

    if (mPhase != Phase::Jumping)
        return;
    const bool wet = inWater(level, mob);
    if (!wet)
        mWaterLandingTick = 0;
    if ((now > mWaterLandingTick && wet) || mob.isOnGround())
        mPhase = Phase::Done;
}
