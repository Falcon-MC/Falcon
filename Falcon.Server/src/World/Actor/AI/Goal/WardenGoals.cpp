#include "World/Actor/AI/Goal/WardenGoals.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/DamageCause.h"
#include "World/Actor/Definition/EntityEvents.h"
#include "World/Actor/Mob/Hostile/WardenActor.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Systems/LiquidPhysicsSystem.h"
#include "World/Level/Level.h"
#include "World/Level/Particle/SonicExplosionParticle.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

namespace {
    const float TICKS_PER_SECOND = 20.0f;
    const float EYE_HEIGHT_RATIO = 0.85f;
    // played when it digs straight away out of water or lava
    const char *const DIG_AWAY_SOUND = "agitated";
    const char *const SILENT_SOUND = "undefined";
    // the component documentation's defaults, for what the warden's definition leaves out
    const float EMERGE_DEFAULT_COOLDOWN = 0.5f;
    const float SONIC_DEFAULT_RANGE_HORIZONTAL = 15.0f;
    const float SONIC_DEFAULT_RANGE_VERTICAL = 20.0f;
    const float SONIC_DEFAULT_SOUND_DELAY = 1.7f;
    // the goal is made with a radius of 1.5: it keeps that one when the definition gives none
    const float INVESTIGATE_DEFAULT_GOAL_RADIUS = 1.5f;
    // the line of puffs runs this many blocks past the target
    const int32_t SONIC_PARTICLE_OVERSHOOT = 6;
    // knockback powers are scaled to motion
    const float SONIC_KNOCKBACK_SCALE = 0.4f;
    const float SONIC_KNOCKBACK_SLOWDOWN = 0.5f;
    const float MIN_DIRECTION_LENGTH = 0.0001f;
    // daylight: strictly brighter than this, with the sky darkened at most this much
    const float DAYLIGHT_BRIGHTNESS = 0.5f;
    const int32_t DAYLIGHT_MAX_SKY_DARKEN = 3;
    const float MAX_LIGHT = 15.0f;
    const float LIQUID_BOX_INSET = 0.001f;

    std::mt19937 &wardenRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    float numberOf(const json::Value &component, const char *key, float fallback) {
        const json::Value *value = component.get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }

    bool flagOf(const json::Value &component, const char *key) {
        const json::Value *value = component.get(key);
        return value != nullptr && value->boolean(false);
    }

    std::string stringOf(const json::Value &component, const char *key) {
        const json::Value *value = component.get(key);
        return value == nullptr ? std::string() : value->string();
    }

    std::shared_ptr<json::Value> triggerOf(const json::Value &component, const char *key) {
        const json::Value *trigger = component.get(key);
        return trigger == nullptr ? nullptr : std::shared_ptr<json::Value>(trigger->clone());
    }

    int64_t ticksOf(float seconds) {
        return (int64_t) std::lround(seconds * TICKS_PER_SECOND);
    }

    void setFlag(ServerNetworkHandler &owner, MobActor &mob, ActorFlag flag, bool on) {
        if (mob.getFlags().get(flag) == on)
            return;
        mob.getFlags().set(flag, on);
        owner.syncActorFlags(mob);
    }

    bool hasLiveTarget(ServerNetworkHandler &owner, const MobActor &mob) {
        return mob.getTarget(owner) != nullptr;
    }

    Vector3i blockOf(const Vector3f &position) {
        return Vector3i((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                        (int32_t) std::floor(position.z));
    }

    Vector3f eyesOf(const Actor &actor) {
        const Vector3f position = actor.getPosition();
        if (dynamic_cast<const ServerPlayer *>(&actor) != nullptr)
            return Vector3f(position.x, position.y + MovementPhysics::PLAYER_EYE_HEIGHT, position.z);
        const ServerActor *server = dynamic_cast<const ServerActor *>(&actor);
        if (server == nullptr)
            return position;
        return Vector3f(position.x, position.y + server->getSize().mHeight * EYE_HEIGHT_RATIO, position.z);
    }

    Vector3f bodyOf(const ServerActor &actor) {
        const Vector3f position = actor.getPosition();
        return Vector3f(position.x, position.y + actor.getSize().mHeight * 0.5f, position.z);
    }

    // inside a cylinder: strictly within the horizontal and the vertical radius
    bool withinCylinder(const Actor &from, const Actor &to, float horizontal, float vertical) {
        const Vector3f &a = from.getPosition();
        const Vector3f &b = to.getPosition();
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float dz = b.z - a.z;
        return dx * dx + dz * dz < horizontal * horizontal && dy * dy < vertical * vertical;
    }

    template <typename Test>
    bool boxHasLiquid(Level &level, const ServerActor &actor, Test test) {
        const Vector3f position = actor.getPosition();
        const ActorSize size = actor.getSize();
        const float half = size.mWidth * 0.5f - LIQUID_BOX_INSET;
        for (int32_t x = (int32_t) std::floor(position.x - half); x <= (int32_t) std::floor(position.x + half); ++x) {
            for (int32_t y = (int32_t) std::floor(position.y + LIQUID_BOX_INSET);
                 y <= (int32_t) std::floor(position.y + size.mHeight - LIQUID_BOX_INSET); ++y) {
                for (int32_t z = (int32_t) std::floor(position.z - half); z <= (int32_t) std::floor(position.z + half);
                     ++z) {
                    if (test(level.getLiquidInfo(x, y, z)))
                        return true;
                }
            }
        }
        return false;
    }

    // open sky above the box top and bright enough, rain dimming it
    bool inDaylight(Level &level, const ServerActor &actor) {
        if (!level.hasSkyLight() || level.getSkyLightSubtracted() > DAYLIGHT_MAX_SKY_DARKEN)
            return false;
        const Vector3f position = actor.getPosition();
        const Vector3i block = blockOf(position);
        const float rain = level.isRaining() ? (float) level.getSkyLightSubtracted() / MAX_LIGHT : 0.0f;
        const float brightness =
                (float) std::max(0, level.getSkyLightAt(block.x, block.y, block.z) - level.getSkyLightSubtracted())
                / MAX_LIGHT;
        if (brightness - rain <= DAYLIGHT_BRIGHTNESS)
            return false;
        const int32_t top = (int32_t) std::round(position.y + actor.getSize().mHeight);
        return top >= level.getHeightAt(block.x, block.z);
    }
}

EmergeGoal::EmergeGoal(Settings settings) : mSettings(std::move(settings)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

bool EmergeGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) mob;
    return owner.getCurrentTick() >= mNextUseTick;
}

bool EmergeGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) mob;
    return owner.getCurrentTick() <= mEndTick;
}

void EmergeGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mEndTick = owner.getCurrentTick() + mSettings.mDurationTicks;
    setFlag(owner, mob, ActorFlag::Emerging, true);
}

void EmergeGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    EntityEvents::fireTrigger(owner, mob, mSettings.mOnDone.get());
    mNextUseTick = owner.getCurrentTick() + mSettings.mCooldownTicks;
    setFlag(owner, mob, ActorFlag::Emerging, false);
}

DigGoal::DigGoal(Settings settings) : mSettings(std::move(settings)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

bool DigGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    const int64_t now = owner.getCurrentTick();
    if (mCreatedTick < 0)
        mCreatedTick = now;
    if (mob.getFlags().get(ActorFlag::Digging))
        return false;
    if (!mob.getNameTag().empty() && !mSettings.mAllowDigWhenNamed)
        return false;

    Level &level = owner.getLevelFor(mob);
    const bool inWater = boxHasLiquid(level, mob, [](const LiquidInfo &liquid) { return liquid.water; });
    const bool inLava = boxHasLiquid(level, mob, [](const LiquidInfo &liquid) { return liquid.lava; });
    if (!mob.isOnGround() && !inWater && !inLava)
        return false;
    if (mSettings.mDigsInDaylight && inDaylight(level, mob))
        return true;

    if (mSettings.mVibrationIsDisturbance) {
        const WardenActor *warden = dynamic_cast<const WardenActor *>(&mob);
        if (warden != nullptr && warden->getLastVibration().has_value()
            && now - warden->getLastVibrationTick() < mSettings.mIdleTicks)
            return false;
    }
    if (mSettings.mSuspicionIsDisturbance && mob.getSuspectPosition().has_value()
        && now - mob.getSuspectTick() < mSettings.mIdleTicks)
        return false;
    return now - mCreatedTick >= mSettings.mIdleTicks;
}

bool DigGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    return owner.getCurrentTick() <= mEndTick && !mob.isExpired();
}

void DigGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mEndTick = owner.getCurrentTick() + mSettings.mDurationTicks;
    Level &level = owner.getLevelFor(mob);
    // no ground to dig into: it leaves at once
    if (boxHasLiquid(level, mob, [](const LiquidInfo &liquid) { return liquid.water || liquid.lava; })) {
        mob.playDefinitionSound(owner, DIG_AWAY_SOUND);
        mob.despawn();
        return;
    }
    setFlag(owner, mob, ActorFlag::Digging, true);
    EntityEvents::fireTrigger(owner, mob, mSettings.mOnStart.get());
}

void DigGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.despawn();
}

RoarGoal::RoarGoal(int64_t durationTicks) : mDurationTicks(durationTicks) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

bool RoarGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (hasLiveTarget(owner, mob) || mob.getComponent("minecraft:anger_level") == nullptr)
        return false;
    const MobAngerLevel &anger = mob.getAngerLevel();
    const Actor *active = anger.getActiveEntity(owner, mob);
    if (active == nullptr || anger.getAngerAt(active->getUniqueId()) < MobAngerLevel::angryThreshold(mob))
        return false;
    mTargetId = active->getUniqueId();
    return true;
}

bool RoarGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    return !hasLiveTarget(owner, mob) && owner.getCurrentTick() <= mEndTick;
}

void RoarGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mEndTick = owner.getCurrentTick() + mDurationTicks;
    setFlag(owner, mob, ActorFlag::Roaring, true);
}

void RoarGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (const Actor *target = owner.getEntity(mTargetId))
        mob.getLookControl().setLookAt(eyesOf(*target));
}

// done roaring: the one it roared at becomes its target, angrier still
void RoarGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    setFlag(owner, mob, ActorFlag::Roaring, false);
    Actor *target = owner.getEntity(mTargetId);
    if (target == nullptr || !target->isAlive())
        return;
    mob.setTarget(owner, target->getRuntimeId());
    mob.getAngerLevel().increase(owner, mob, *target, MobAngerLevel::angryBoost(mob), false);
}

SniffGoal::SniffGoal(Settings settings) : mSettings(settings) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

Actor *SniffGoal::findCandidate(ServerNetworkHandler &owner, MobActor &mob) const {
    const float radius = mSettings.mSniffingRadius;
    const AxisAlignedBB area = ActorPushSystem::boundingBoxOf(mob).expand(radius, radius, radius);
    const Vector3f &from = mob.getPosition();
    const MobAngerLevel &anger = mob.getAngerLevel();
    const auto closest = [&](auto &&visit) {
        Actor *best = nullptr;
        float bestDistance = std::numeric_limits<float>::max();
        visit([&](Actor &actor) {
            if (&actor == &mob || !area.intersectsWith(ActorPushSystem::boundingBoxOf(actor))
                || !anger.isValidTarget(owner, mob, actor))
                return;
            const Vector3f &at = actor.getPosition();
            const float distance = (at.x - from.x) * (at.x - from.x) + (at.y - from.y) * (at.y - from.y)
                                   + (at.z - from.z) * (at.z - from.z);
            if (distance < bestDistance) {
                bestDistance = distance;
                best = &actor;
            }
        });
        return best;
    };

    Level &level = owner.getLevelFor(mob);
    Actor *player = closest([&](auto &&check) {
        for (auto &entry: owner.getPlayers()) {
            ServerPlayer &candidate = entry.second;
            if (candidate.isSpawned() && &owner.getLevelFor(candidate) == &level)
                check(candidate);
        }
    });
    if (player != nullptr)
        return player;
    return closest([&](auto &&check) {
        owner.forEachActorNear(from, radius + mob.getSize().mWidth, [&](ServerActor &actor) {
            if (dynamic_cast<MobActor *>(&actor) != nullptr && &owner.getLevelFor(actor) == &level)
                check(actor);
        });
    });
}

bool SniffGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (hasLiveTarget(owner, mob) || owner.getCurrentTick() < mNextUseTick)
        return false;
    if (mob.getSuspectPosition().has_value())
        return false;
    return findCandidate(owner, mob) != nullptr;
}

bool SniffGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) mob;
    return owner.getCurrentTick() <= mEndTick;
}

void SniffGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mEndTick = owner.getCurrentTick() + mSettings.mDurationTicks;
    setFlag(owner, mob, ActorFlag::Sniffing, true);
}

// the sniff ends on whoever is closest now: suspected, and angered when within the suspicion cylinder
void SniffGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    const int64_t now = owner.getCurrentTick();
    if (Actor *candidate = findCandidate(owner, mob)) {
        mob.setSuspectPosition(blockOf(candidate->getPosition()), now);
        if (withinCylinder(mob, *candidate, mSettings.mSuspicionRadiusHorizontal, mSettings.mSuspicionRadiusVertical))
            mob.getAngerLevel().increase(owner, mob, *candidate, MobAngerLevel::defaultAnnoyingness(mob), true);
    }
    setFlag(owner, mob, ActorFlag::Sniffing, false);

    const float roll = std::uniform_real_distribution<float>(0.0f, 1.0f)(wardenRandom());
    const float seconds = mSettings.mCooldownMin + roll * (mSettings.mCooldownMax - mSettings.mCooldownMin);
    mNextUseTick = now + (uint16_t) (int32_t) (seconds * TICKS_PER_SECOND);
}

InvestigateSuspiciousLocationGoal::InvestigateSuspiciousLocationGoal(float speed, float goalRadius)
        : mSpeed(speed), mGoalRadiusSquared(goalRadius * goalRadius) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

// the suspect block, unless the mob already stands close enough to it
std::optional<Vector3i> InvestigateSuspiciousLocationGoal::_suspect(const MobActor &mob) const {
    const std::optional<Vector3i> &suspect = mob.getSuspectPosition();
    if (!suspect.has_value())
        return std::nullopt;
    const Vector3f &at = mob.getPosition();
    const float dx = (float) suspect->x + 0.5f - at.x;
    const float dy = (float) suspect->y + 0.5f - at.y;
    const float dz = (float) suspect->z + 0.5f - at.z;
    if (dx * dx + dy * dy + dz * dz <= mGoalRadiusSquared)
        return std::nullopt;
    return suspect;
}

bool InvestigateSuspiciousLocationGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (hasLiveTarget(owner, mob) || mob.isRiding())
        return false;
    const std::optional<Vector3i> suspect = _suspect(mob);
    if (!suspect.has_value())
        return false;
    mLocation = *suspect;
    return true;
}

bool InvestigateSuspiciousLocationGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (hasLiveTarget(owner, mob))
        return false;
    const std::optional<Vector3i> &suspect = mob.getSuspectPosition();
    return suspect.has_value() && *suspect == mLocation && !mFailed && !mReached;
}

void InvestigateSuspiciousLocationGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mFailed = false;
    mReached = false;
    mRepathed = false;
    mob.getNavigation().moveTo(Vector3f((float) mLocation.x, (float) mLocation.y, (float) mLocation.z), mSpeed);
}

void InvestigateSuspiciousLocationGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    PathNavigation &navigation = mob.getNavigation();
    if (!navigation.isDone() || mFailed)
        return;
    const Vector3f &at = mob.getPosition();
    const float dx = (float) mLocation.x + 0.5f - at.x;
    const float dy = (float) mLocation.y + 0.5f - at.y;
    const float dz = (float) mLocation.z + 0.5f - at.z;
    if (dx * dx + dy * dy + dz * dz <= mGoalRadiusSquared) {
        mReached = true;
        return;
    }
    // the path ended short: one more try, then it gives up
    if (mRepathed) {
        mFailed = true;
        return;
    }
    mRepathed = true;
    navigation.moveTo(Vector3f((float) mLocation.x, (float) mLocation.y, (float) mLocation.z), mSpeed);
}

void InvestigateSuspiciousLocationGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    const std::optional<Vector3i> &suspect = mob.getSuspectPosition();
    if (suspect.has_value() && *suspect == mLocation)
        mob.clearSuspectPosition();
    mob.getNavigation().stop(mob);
    mFailed = false;
    mReached = false;
}

SonicBoomGoal::SonicBoomGoal(Settings settings) : mSettings(std::move(settings)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

bool SonicBoomGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    Actor *target = mob.getTarget(owner);
    if (target == nullptr || !target->isAlive())
        return false;
    // nothing to hurt on peaceful: the target is let go
    if (target->isPlayer() && owner.getProperties().getDifficulty() == Difficulty::Peaceful) {
        mob.setTarget(owner, 0);
        return false;
    }
    if (mCooldownEndTick.has_value() && owner.getCurrentTick() <= *mCooldownEndTick)
        return false;
    return withinCylinder(mob, *target, mSettings.mRangeHorizontal, mSettings.mRangeVertical);
}

bool SonicBoomGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mEndTick.has_value() && owner.getCurrentTick() > *mEndTick)
        return false;
    return canUse(owner, mob);
}

void SonicBoomGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    Actor *target = mob.getTarget(owner);
    if (target == nullptr)
        return;
    mob.getNavigation().stop(mob);
    setFlag(owner, mob, ActorFlag::SonicBoom, true);
    const int64_t now = owner.getCurrentTick();
    mAttackTick = now + mSettings.mAttackDelayTicks;
    mEndTick = now + mSettings.mDurationTicks;
    mob.getLookControl().setLookAt(eyesOf(*target));
    if (mSettings.mChargeSound != SILENT_SOUND && !mob.getFlags().get(ActorFlag::Silent))
        mob.playDefinitionSound(owner, mSettings.mChargeSound);
}

void SonicBoomGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    Actor *target = mob.getTarget(owner);
    if (target == nullptr)
        return;
    mob.getNavigation().moveTo(target->getPosition(), mSettings.mSpeed);
    if (mAttackTick.has_value() && owner.getCurrentTick() <= *mAttackTick)
        return;
    _boom(owner, mob, *target);
}

void SonicBoomGoal::_boom(ServerNetworkHandler &owner, MobActor &mob, Actor &target) {
    const Vector3f from = bodyOf(mob);
    const Vector3f to = eyesOf(target);
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float dz = to.z - from.z;
    const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
    Vector3f direction(0.0f, 0.0f, 0.0f);
    if (length >= MIN_DIRECTION_LENGTH)
        direction = Vector3f(dx / length, dy / length, dz / length);

    Level &level = owner.getLevelFor(mob);
    const int32_t steps = (int32_t) std::floor(length) + SONIC_PARTICLE_OVERSHOOT;
    for (int32_t step = 1; step <= steps; ++step) {
        const float along = (float) step;
        level.addParticle(SonicExplosionParticle(
                Vector3f(from.x + direction.x * along, from.y + direction.y * along, from.z + direction.z * along)));
    }

    // one boom per use: the next would fall after the goal ends
    mAttackTick = owner.getCurrentTick() + (mSettings.mDurationTicks - mSettings.mAttackDelayTicks);
    if (mSettings.mAttackSound != SILENT_SOUND && !mob.getFlags().get(ActorFlag::Silent))
        mob.playDefinitionSound(owner, mSettings.mAttackSound);

    const ActorDamageSource source = ActorDamageSource::attack(DamageCause::findDeathMessageKey("sonic_boom"),
                                                               target.getName(), mob, mob.getName(),
                                                               mob.getPosition())
                                             .withCause("sonic_boom");
    if (ServerPlayer *player = dynamic_cast<ServerPlayer *>(&target))
        owner.hurt(*player, mSettings.mDamage, source);
    else if (ServerActor *actor = dynamic_cast<ServerActor *>(&target))
        owner.damageActor(*actor, mSettings.mDamage, source);

    if (!target.isPlayer() && dynamic_cast<MobActor *>(&target) == nullptr)
        return;
    target.knockBack(dx, dz, mSettings.mKnockbackHorizontal * SONIC_KNOCKBACK_SCALE,
                     direction.y * mSettings.mKnockbackVertical * SONIC_KNOCKBACK_SCALE, mSettings.mKnockbackHeightCap,
                     SONIC_KNOCKBACK_SLOWDOWN);
}

void SonicBoomGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    setFlag(owner, mob, ActorFlag::SonicBoom, false);
    mCooldownEndTick = owner.getCurrentTick() + mSettings.mCooldownTicks;
    mEndTick.reset();
    mAttackTick.reset();
}

std::unique_ptr<Goal> WardenGoals::create(const MobActor &mob, const std::string &behavior,
                                          const json::Value &component) {
    if (behavior == "emerge") {
        EmergeGoal::Settings settings;
        settings.mDurationTicks = ticksOf(numberOf(component, "duration", 0.0f));
        settings.mCooldownTicks = ticksOf(numberOf(component, "cooldown_time", EMERGE_DEFAULT_COOLDOWN));
        settings.mOnDone = triggerOf(component, "on_done");
        return std::make_unique<EmergeGoal>(std::move(settings));
    }

    if (behavior == "dig") {
        DigGoal::Settings settings;
        settings.mDurationTicks = ticksOf(numberOf(component, "duration", 0.0f));
        settings.mIdleTicks = ticksOf(numberOf(component, "idle_time", 0.0f));
        settings.mAllowDigWhenNamed = flagOf(component, "allow_dig_when_named");
        settings.mDigsInDaylight = flagOf(component, "digs_in_daylight");
        settings.mVibrationIsDisturbance = flagOf(component, "vibration_is_disturbance");
        settings.mSuspicionIsDisturbance = flagOf(component, "suspicion_is_disturbance");
        settings.mOnStart = triggerOf(component, "on_start");
        return std::make_unique<DigGoal>(std::move(settings));
    }

    if (behavior == "roar")
        return std::make_unique<RoarGoal>(ticksOf(numberOf(component, "duration", 0.0f)));

    if (behavior == "sniff") {
        SniffGoal::Settings settings;
        settings.mDurationTicks = ticksOf(numberOf(component, "duration", 0.0f));
        settings.mSniffingRadius = numberOf(component, "sniffing_radius", 0.0f);
        settings.mSuspicionRadiusHorizontal =
                numberOf(component, "suspicion_radius_horizontal", 0.0f);
        settings.mSuspicionRadiusVertical =
                numberOf(component, "suspicion_radius_vertical", 0.0f);
        if (const json::Value *range = component.get("cooldown_range")) {
            settings.mCooldownMin = numberOf(*range, "min", 0.0f);
            settings.mCooldownMax = numberOf(*range, "max", 0.0f);
        }
        return std::make_unique<SniffGoal>(settings);
    }

    if (behavior == "investigate_suspicious_location")
        return std::make_unique<InvestigateSuspiciousLocationGoal>(
                mob.getMovementSpeed() * numberOf(component, "speed_multiplier", 1.0f),
                numberOf(component, "goal_radius", INVESTIGATE_DEFAULT_GOAL_RADIUS));

    if (behavior == "sonic_boom") {
        SonicBoomGoal::Settings settings;
        settings.mDurationTicks = ticksOf(numberOf(component, "duration", 0.0f));
        settings.mAttackDelayTicks =
                ticksOf(numberOf(component, "duration_until_attack_sound", SONIC_DEFAULT_SOUND_DELAY));
        settings.mCooldownTicks = ticksOf(numberOf(component, "attack_cooldown", 0.0f));
        settings.mSpeed = mob.getMovementSpeed() * numberOf(component, "speed_multiplier", 1.0f);
        settings.mDamage = numberOf(component, "attack_damage", 0.0f);
        settings.mRangeHorizontal = numberOf(component, "attack_range_horizontal", SONIC_DEFAULT_RANGE_HORIZONTAL);
        settings.mRangeVertical = numberOf(component, "attack_range_vertical", SONIC_DEFAULT_RANGE_VERTICAL);
        settings.mKnockbackHorizontal = numberOf(component, "knockback_horizontal_strength", 0.0f);
        settings.mKnockbackVertical = numberOf(component, "knockback_vertical_strength", 0.0f);
        settings.mKnockbackHeightCap = numberOf(component, "knockback_height_cap", 0.0f);
        settings.mAttackSound = stringOf(component, "attack_sound");
        settings.mChargeSound = stringOf(component, "charge_sound");
        if (settings.mAttackSound.empty())
            settings.mAttackSound = SILENT_SOUND;
        if (settings.mChargeSound.empty())
            settings.mChargeSound = SILENT_SOUND;
        return std::make_unique<SonicBoomGoal>(std::move(settings));
    }

    return nullptr;
}
