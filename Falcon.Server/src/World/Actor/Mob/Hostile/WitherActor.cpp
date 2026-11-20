#include "World/Actor/Mob/Hostile/WitherActor.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/BossEventPacket.h"
#include "Protocol/Packets/LegacyTelemetryEventPacket.h"
#include "Server/PropertiesSettings.h"
#include "World/Actor/AI/Goal/MobAttack.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/DamageCause.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/Component/MobShooter.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/Projectile/ProjectileActor.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Explosion.h"
#include "World/Level/Level.h"
#include "Core/Math/VectorMath.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

FALCON_REGISTER_ACTOR(WitherActor, WitherActor::IDENTIFIER);

namespace {
    const float DEGREES_TO_RADIANS = 0.017453292f;
    const float RADIANS_TO_DEGREES = 57.295776f;

    // the synced data the client reads the wither from
    const int32_t INVULNERABLE_TICKS_DATA_ID = 48;
    const int32_t FIRST_HEAD_TARGET_DATA_ID = 49;
    const int32_t AIR_ATTACK_DATA_ID = 52;
    const int32_t SPAWNING_FRAMES_DATA_ID = 105;

    const char *const TAG_INVULNERABLE = "Invul";
    const char *const TAG_AIR_ATTACK = "AirAttack";
    const char *const TAG_SHIELD_HEALTH = "ShieldHealth";
    const char *const TAG_PHASE = "Phase";
    const char *const TAG_SPAWNING_FRAMES = "SpawningFrames";
    const char *const TAG_MAX_HEALTH = "maxHealth";
    const char *const TAG_LAST_HEALTH_INTERVAL = "lastHealthInterval";
    const char *const TAG_FIRE_RATE = "firerate";

    const char *const GRIEFING_RULE = "mobgriefing";
    const char *const BOSS_COMPONENT = "minecraft:boss";
    const char *const SKULL_IDENTIFIER = "minecraft:wither_skull";
    const char *const DANGEROUS_SKULL_IDENTIFIER = "minecraft:wither_skull_dangerous";
    const char *const SKELETON_IDENTIFIER = "minecraft:wither_skeleton";

    // the game sets the movement attribute to it when the spawn frames end
    const float MOVEMENT_SPEED = 0.6f;
    // the follow range attribute the spawn sets
    const float FOLLOW_RANGE = 70.0f;
    const float EXPLOSION_POWER = 7.0f;
    const float CHARGE_DAMAGE = 15.0f;
    const int32_t SKELETON_COUNT = 3;
    const int32_t FROZEN_TICKS = 80;
    const float PLAYER_SEARCH_HEIGHT = 8.0f;
    // no player in this box: one health every 20 ticks
    const float REGENERATION_RANGE = 50.0f;

    const float BOSS_HUD_RANGE = 55.0f;
    const int32_t BOSS_ACTION_CREATE = 0;
    const int32_t BOSS_ACTION_REMOVE = 2;
    const int32_t BOSS_ACTION_UPDATE_PERCENT = 4;
    // the colour the game's boss component sends for every boss: red
    const int32_t BOSS_COLOR_RED = 2;

    const int32_t PARTICLE_EVENT = 2009;
    const int32_t SUMMON_PARTICLE = 15;
    const int32_t SUMMON_PARTICLE_COUNT = 120;

    // the players in this box around the new wither get the achievement
    const float ACHIEVEMENT_RANGE = 50.5f;
    const float ACHIEVEMENT_RANGE_Z = 51.75f;
    const int32_t SPAWN_THE_WITHER_ACHIEVEMENT = 62;
    // the low byte of the wither's actor type, as the boss killed event carries it
    const int32_t WITHER_ACTOR_TYPE = 52;
    // the hurt cooldown the game gives the wither at spawn and the skeletons it summons
    const int32_t SPAWN_NO_DAMAGE_TICKS = 200;
    const int32_t SKELETON_NO_DAMAGE_TICKS = 60;

    using VectorMath::wrapDegrees;

    float clampf(float value, float minimum, float maximum) {
        return std::max(minimum, std::min(maximum, value));
    }

    // the game aims at the actor's position, a player's being its eyes
    Vector3f aimPosition(const Actor &actor) {
        Vector3f position = actor.getPosition();
        if (actor.isPlayer())
            position.y += MovementPhysics::PLAYER_EYE_HEIGHT;
        return position;
    }

    float distanceSquared(const Vector3f &left, const Vector3f &right) {
        const float dx = left.x - right.x;
        const float dy = left.y - right.y;
        const float dz = left.z - right.z;
        return dx * dx + dy * dy + dz * dz;
    }

    // the responsible actor: the attacker, else the damager, a projectile standing for its owner
    Actor *responsibleOf(ServerNetworkHandler &owner, const ActorDamageSource &source) {
        if (source.mAttacker != nullptr)
            return source.mAttacker;
        ServerActor *damager = dynamic_cast<ServerActor *>(source.mDamager);
        if (damager == nullptr)
            return source.mDamager;
        if (damager->isProjectile() && damager->getOwnerUniqueId() != -1) {
            Actor *shooter = owner.getEntity(damager->getOwnerUniqueId());
            if (shooter != nullptr)
                return shooter;
        }
        return damager;
    }

    struct SkullSettings {
        float mPower = 1.2f;
        float mUncertaintyBase = 0.0f;
        float mUncertaintyMultiplier = 0.0f;
        Vector3f mOffset;
    };

    SkullSettings skullSettings(const std::string &identifier) {
        SkullSettings settings;
        const json::Value *definition = EntityDefinitions::find(identifier);
        const json::Value *components = definition == nullptr ? nullptr : definition->get("components");
        const json::Value *projectile = components == nullptr ? nullptr : components->get("minecraft:projectile");
        if (projectile == nullptr)
            return settings;

        const auto number = [projectile](const char *key, float fallback) {
            const json::Value *value = projectile->get(key);
            return value == nullptr ? fallback : (float) value->number(fallback);
        };
        settings.mPower = number("power", settings.mPower);
        settings.mUncertaintyBase = number("uncertainty_base", settings.mUncertaintyBase);
        settings.mUncertaintyMultiplier = number("uncertainty_multiplier", settings.mUncertaintyMultiplier);
        const json::Value *offset = projectile->get("offset");
        if (offset != nullptr && offset->isArray() && offset->mArray.size() == 3)
            settings.mOffset = Vector3f((float) offset->mArray[0]->number(0.0), (float) offset->mArray[1]->number(0.0),
                                        (float) offset->mArray[2]->number(0.0));
        return settings;
    }
}

WitherActor::WitherActor(uint64_t runtimeId, const std::string &identifier)
        : HostileActor(runtimeId, identifier), mRandom(std::random_device{}()) {}

float WitherActor::resolveMaxHealth(Difficulty difficulty) const {
    switch (difficulty) {
        case Difficulty::Hard:
            return HARD_HEALTH;
        case Difficulty::Normal:
            return NORMAL_HEALTH;
        default:
            return EASY_HEALTH;
    }
}

float WitherActor::getMovementSpeed() const {
    return MOVEMENT_SPEED;
}

float WitherActor::getFollowRange() const {
    return FOLLOW_RANGE;
}

void WitherActor::finalizeSpawn() {
    HostileActor::finalizeSpawn();
    mSpawnPending = true;
}

int32_t WitherActor::_nextInt(int32_t bound) {
    if (bound <= 0)
        return 0;
    return std::uniform_int_distribution<int32_t>(0, bound - 1)(mRandom);
}

float WitherActor::_nextFloat() {
    return std::uniform_real_distribution<float>(0.0f, 1.0f)(mRandom);
}

void WitherActor::_startSpawning(ServerNetworkHandler &owner) {
    mSpawnPending = false;
    mAirAttack = true;
    mSpawningFrames = SPAWNING_FRAMES;
    mInvulnerableTicks = SPAWNING_FRAMES;

    // the shield follows the health the difficulty scaled
    const Difficulty difficulty = owner.getProperties().getDifficulty();
    if (difficulty == Difficulty::Easy)
        mMaxShieldHealth = (int32_t) ((float) mMaxShieldHealth * 0.5f);
    else if (difficulty == Difficulty::Normal)
        mMaxShieldHealth = (int32_t) ((float) mMaxShieldHealth * 0.75f);

    mSavedMaxHealth = (int32_t) getMaxHealth();
    setNoDamageTicks(SPAWN_NO_DAMAGE_TICKS);
    setHealth(1.0f);
    mLastHealthStep = (int32_t) getMaxHealth();
    owner.syncActorAttributes(*this);
}

void WitherActor::_initializeHealthSteps() {
    mHealthStepsReady = true;
    const int32_t maxHealth = (int32_t) getMaxHealth();
    mPhaseHealth = maxHealth / 2;
    mHealthStep = maxHealth / 6;
}

void WitherActor::tick(ServerNetworkHandler &owner) {
    if (mSpawnPending)
        _startSpawning(owner);
    if (!mHealthStepsReady) {
        _initializeHealthSteps();
        _sendData(owner, SPAWNING_FRAMES_DATA_ID);
        _sendData(owner, INVULNERABLE_TICKS_DATA_ID);
        _sendData(owner, AIR_ATTACK_DATA_ID);
    }

    HostileActor::tick(owner);
    _tickBossBar(owner);
}

void WitherActor::tickMovement(ServerNetworkHandler &owner) {
    Level &level = owner.getLevelFor(*this);
    _preAiStep(owner);
    if (!isDead() && getHealth() > 0.0f)
        _serverAiStep(owner, level);
    HostileActor::tickMovement(owner);
}

Vector3f WitherActor::_headPosition(int32_t head) const {
    const Vector3f position = getPosition();
    if (head <= 0)
        return Vector3f(position.x, position.y + 3.0f, position.z);

    const float angle = (getRotation().y + (float) (180 * head - 180)) * -DEGREES_TO_RADIANS;
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    return Vector3f(position.x + cosine * 1.3f, position.y + 2.2f, position.z - sine * 1.3f);
}

void WitherActor::_preAiStep(ServerNetworkHandler &owner) {
    if (mAttackCooldownTicks > 0)
        mAttackCooldownTicks--;

    // a hit by its own kind of skull, or the half health blast, holds it still for a while
    if (mFrozenTicks-- > 1)
        return;

    Vector3f motion = getMotion();
    motion.y *= 0.6f;
    setMotion(motion);

    if (mSpawningFrames > 0 && getHealth() > 0.0f) {
        _tickSpawning(owner, owner.getLevelFor(*this));
        return;
    }

    const Vector3f position = getPosition();
    Actor *target = mHeadTargets[0] == 0 ? nullptr : MobActor::findActor(owner, mHeadTargets[0]);
    if (target != nullptr) {
        motion = getMotion();
        const float targetY = aimPosition(*target).y;
        if (mFlyingToAttackPosition) {
            if (getMoveControl().hasWanted())
                motion.y = getMoveControl().getWantedPosition().y - position.y;
        } else if ((targetY > position.y && !mHalfHealthExplosionPending)
                   || (mAirAttack && targetY + 5.0f > position.y)) {
            const float rising = std::max(0.0f, motion.y);
            motion.y = (0.5f - rising) * 0.6f + rising;
        } else if (mHalfHealthExplosionPending) {
            motion.y -= 1.0f;
        }
        setMotion(motion);
    }

    if (mChargeTicks > 0) {
        motion = getMotion();
        motion.x = mChargeDirection.x + mChargeDirection.x;
        motion.z = mChargeDirection.z + mChargeDirection.z;
        if (--mChargeTicks == 0) {
            mCharging = false;
            mAttackCooldownTicks = 20;
            mChargeDirection.x = 0.0f;
            motion.x = 0.0f;
            motion.z = 0.0f;
        }
        setMotion(motion);
    }

    // turns its body at most 10 degrees a tick toward the target
    if (target != nullptr) {
        const Vector3f aim = aimPosition(*target);
        Vector3f rotation = getRotation();
        const float towards = std::atan2(aim.z - position.z, aim.x - position.x) * RADIANS_TO_DEGREES - 90.0f;
        rotation.y += clampf(wrapDegrees(towards - rotation.y), -10.0f, 10.0f);
        setRotation(rotation);
    }
}

void WitherActor::_tickSpawning(ServerNetworkHandler &owner, Level &level) {
    if (mSpawningFrames-- == 1) {
        _setAirAttack(owner, true);
        mShieldHealth = mMaxShieldHealth;
        // the wither has no offset on the attach point the game explodes at: its own position
        _explode(owner, level, getPosition());
        owner.playLevelSound(level, "spawn", getPosition(), IDENTIFIER);
    }

    _setInvulnerableTicks(owner, mSpawningFrames);
    heal(std::ceil(getMaxHealth() / 200.0f));
    owner.syncActorAttributes(*this);

    // spins faster and faster: one spawn sound per turn
    mSpinSpeed += 0.2f;
    Vector3f rotation = getRotation();
    const float previous = wrapDegrees(rotation.y);
    const float yaw = wrapDegrees(previous - mSpinSpeed);
    rotation.y = yaw;
    rotation.z = yaw;
    setRotation(rotation);
    if (previous < 0.0f && yaw >= 0.0f)
        owner.playLevelSound(level, "spawn", getPosition(), IDENTIFIER);
}

void WitherActor::_serverAiStep(ServerNetworkHandler &owner, Level &level) {
    if (mSpawningFrames > 0)
        return;

    if (mHalfHealthExplosionPending)
        _explodeAtHalfHealth(owner, level);

    Actor *target = getTarget(owner);
    if (target != nullptr && mHeadTargets[0] != target->getRuntimeId())
        _setHeadTarget(owner, 0, target->getRuntimeId());

    const bool idle = mAerialAttackTicks == 0 && !mAerialAttackArmed && !mCharging && mChargeWindupTicks == 0
                      && mAttackCooldownTicks <= 0;

    if (mShootTicks > 0 && --mShootTicks == 0) {
        if (idle && target != nullptr) {
            getNavigation().stop(*this);
            mIdleTargetTicks = 0;
            const bool dangerous = (++mShotCount & 3) == 0;
            _shootSkull(owner, level, 0, aimPosition(*target), dangerous);
            if (dangerous) {
                mAerialAttackTicks = mAerialAttackInterval;
                // below half health every other fourth shot leads to a charge
                if (!mAirAttack) {
                    mSkipCharge = !mSkipCharge;
                    if (!mSkipCharge) {
                        mAerialAttackTicks = 0;
                        mAttackCooldownTicks = 60;
                    }
                }
            }
        }
        mShootTicks = mFireRate;
    }

    const bool shoots = (int32_t) owner.getProperties().getDifficulty() >= (int32_t) Difficulty::Normal;
    _tickHeads(owner, level, target, shoots);

    if (mBlockBreakTicks > 0 && --mBlockBreakTicks == 0)
        _breakBlocks(owner, level, ActorPushSystem::boundingBoxOf(*this), 1, false);

    if (mAerialAttackTicks > 0 && --mAerialAttackTicks == 0) {
        if (mAirAttack) {
            mAerialAttackArmed = true;
        } else {
            mChargeWindupTicks = 20;
            if (target != nullptr) {
                const Vector3f position = getPosition();
                const Vector3f other = target->getPosition();
                const Vector3f delta(other.x - position.x, other.y - position.y, other.z - position.z);
                const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
                mChargeDirection = length < 0.0001f ? Vector3f(0.0f, 0.0f, 0.0f)
                                                    : Vector3f(delta.x / length, delta.y / length, delta.z / length);
            }
        }
    }

    if (mCharging)
        _tickCharge(owner, level);

    if (mChargeWindupTicks > 0 && --mChargeWindupTicks == 0 && target != nullptr) {
        mChargeTicks = 10;
        mCharging = true;
    }

    if (getLifetimeTicks() % 20 == 0) {
        const AxisAlignedBB area = ActorPushSystem::boundingBoxOf(*this).expand(REGENERATION_RANGE, REGENERATION_RANGE,
                                                                                REGENERATION_RANGE);
        bool playerNear = false;
        owner.forEachPlayerNear(getPosition(), REGENERATION_RANGE * 2.0f, [&](ServerPlayer &player) {
            if (player.isSpawned() && player.sharesLevelWith(*this)
                && area.intersectsWith(ActorPushSystem::boundingBoxOf(player)))
                playerNear = true;
        });
        if (!playerNear) {
            heal(1.0f);
            owner.syncActorAttributes(*this);
        }
    }

    // a target kept 100 ticks without a dive or a charge triggers one
    if (mAerialAttackTicks == 0 && !mAerialAttackArmed && !mCharging && mChargeWindupTicks == 0
        && mAttackCooldownTicks <= 0 && target != nullptr && mIdleTargetTicks++ >= 100) {
        mAerialAttackTicks = 1;
        mIdleTargetTicks = 0;
    }
}

void WitherActor::_tickHeads(ServerNetworkHandler &owner, Level &level, Actor *target, bool shoots) {
    const int32_t ticks = getLifetimeTicks();
    for (int32_t head = 1; head < HEAD_COUNT; ++head) {
        const int32_t index = head - 1;
        const bool busy = mAerialAttackTicks != 0 || mAerialAttackArmed || mCharging || mChargeWindupTicks != 0
                          || mAttackCooldownTicks > 0;
        if (ticks < mNextHeadUpdate[index] || busy) {
            _setHeadTarget(owner, 0, target == nullptr ? 0 : target->getRuntimeId());
            continue;
        }

        mNextHeadUpdate[index] = ticks + 10 + _nextInt(10);
        if (shoots && mIdleHeadUpdates[index]++ >= 16) {
            // an idle head fires a blue skull at a random spot: only the first side head does
            const Vector3f position = getPosition();
            const float x = position.x - 10.0f + _nextFloat() * 20.0f;
            const float y = position.y - 5.0f + _nextFloat() * 10.0f;
            const float z = position.z - 10.0f + _nextFloat() * 20.0f;
            if (head == 1)
                _shootSkull(owner, level, head, Vector3f(x, y, z), true);
            mIdleHeadUpdates[index] = 0;
        }

        Actor *headTarget = mHeadTargets[head] == 0 ? nullptr : MobActor::findActor(owner, mHeadTargets[head]);
        if (mHeadTargets[head] != 0) {
            const bool valid = headTarget != nullptr && headTarget->isAlive() && canTarget(*headTarget)
                               && distanceSquared(getPosition(), aimPosition(*headTarget)) <= mHeadRange * mHeadRange
                               && canSee(owner, *headTarget);
            if (!valid) {
                _setHeadTarget(owner, head, 0);
            } else if (shoots) {
                _shootSkull(owner, level, head, aimPosition(*headTarget), false);
                mNextHeadUpdate[index] = ticks + 40 + _nextInt(20);
                mIdleHeadUpdates[index] = 0;
            }
            _setHeadTarget(owner, 0, target == nullptr ? 0 : target->getRuntimeId());
            continue;
        }

        // a free head picks one of the players around, never the main target, ten tries at most
        const AxisAlignedBB area = ActorPushSystem::boundingBoxOf(*this).expand(mHeadRange, PLAYER_SEARCH_HEIGHT,
                                                                                mHeadRange);
        std::vector<ServerPlayer *> candidates;
        owner.forEachPlayerNear(getPosition(), mHeadRange + PLAYER_SEARCH_HEIGHT, [&](ServerPlayer &player) {
            if (player.isSpawned() && player.sharesLevelWith(*this)
                && area.intersectsWith(ActorPushSystem::boundingBoxOf(player)))
                candidates.push_back(&player);
        });
        for (int32_t attempt = 0; attempt < 10 && !candidates.empty(); ++attempt) {
            const size_t pick = (size_t) _nextInt((int32_t) candidates.size());
            ServerPlayer *candidate = candidates[pick];
            if (canTarget(*candidate) && candidate != target) {
                _setHeadTarget(owner, head, candidate->getRuntimeId());
                break;
            }
            candidates.erase(candidates.begin() + (std::ptrdiff_t) pick);
        }
        _setHeadTarget(owner, 0, target == nullptr ? 0 : target->getRuntimeId());
    }
}

void WitherActor::_tickCharge(ServerNetworkHandler &owner, Level &level) {
    _breakBlocks(owner, level, ActorPushSystem::boundingBoxOf(*this), 2, true);
    _hurtCharged(owner, level);
}

void WitherActor::_hurtCharged(ServerNetworkHandler &owner, Level &level) {
    (void) level;
    const AxisAlignedBB area = ActorPushSystem::boundingBoxOf(*this).expand(1.0f, 4.0f, 1.0f);
    std::vector<Actor *> victims;
    owner.forEachPlayerNear(getPosition(), 5.0f, [&](ServerPlayer &player) {
        if (player.isSpawned() && player.sharesLevelWith(*this)
            && area.intersectsWith(ActorPushSystem::boundingBoxOf(player)))
            victims.push_back(&player);
    });
    owner.forEachActorNear(getPosition(), 5.0f, [&](ServerActor &actor) {
        if (&actor != this && dynamic_cast<MobActor *>(&actor) != nullptr && actor.isAlive()
            && actor.sharesLevelWith(*this) && area.intersectsWith(ActorPushSystem::boundingBoxOf(actor)))
            victims.push_back(&actor);
    });

    for (Actor *victim: victims)
        MobAttack::hit(owner, *this, *victim, CHARGE_DAMAGE);
}

void WitherActor::_explodeAtHalfHealth(ServerNetworkHandler &owner, Level &level) {
    mHalfHealthExplosionPending = false;
    _explode(owner, level, _headPosition(mPhase));
    mFrozenTicks = FROZEN_TICKS;

    if (owner.getProperties().getDifficulty() == Difficulty::Easy)
        return;

    for (int32_t i = 0; i < SKELETON_COUNT; ++i) {
        ServerActor *skeleton = owner.spawnActor(level, SKELETON_IDENTIFIER, getPosition());
        if (skeleton != nullptr)
            skeleton->setNoDamageTicks(SKELETON_NO_DAMAGE_TICKS);
    }
}

void WitherActor::_explode(ServerNetworkHandler &owner, Level &level, const Vector3f &center) {
    Explosion explosion(owner, level, center, EXPLOSION_POWER, this, false);
    if (level.getGameRules().getBool(GRIEFING_RULE))
        explosion.explode();
    else
        explosion.explodeWithoutBlocks();
}

void WitherActor::_shootSkull(ServerNetworkHandler &owner, Level &level, int32_t head, const Vector3f &target,
                              bool dangerous) {
    owner.playLevelSound(level, "shoot", getPosition(), IDENTIFIER);

    const std::string identifier = dangerous ? DANGEROUS_SKULL_IDENTIFIER : SKULL_IDENTIFIER;
    const SkullSettings settings = skullSettings(identifier);
    const Vector3f origin = _headPosition(head);
    const float dx = target.x - origin.x;
    const float dy = target.y - 0.5f - origin.y;
    const float dz = target.z - origin.z;
    const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (length < 0.0001f)
        return;

    const Vector3f spawn(origin.x + settings.mOffset.x, origin.y + settings.mOffset.y, origin.z + settings.mOffset.z);
    ServerActor *skull = owner.spawnActor(level, identifier, spawn);
    if (skull == nullptr)
        return;

    const float spread = 0.0075f * MobShooter::uncertainty(settings.mUncertaintyBase, settings.mUncertaintyMultiplier,
                                                           (int32_t) owner.getProperties().getDifficulty());
    std::normal_distribution<float> gaussian(0.0f, 1.0f);
    const Vector3f direction(dx / length + gaussian(mRandom) * spread, dy / length + gaussian(mRandom) * spread,
                             dz / length + gaussian(mRandom) * spread);

    skull->setProjectile(true);
    skull->setOwnerUniqueId(getUniqueId());
    skull->setMotion(
            Vector3f(direction.x * settings.mPower, direction.y * settings.mPower, direction.z * settings.mPower));
    owner.allowProjectileLaunch(*skull, this);
}

void WitherActor::_setHeadTarget(ServerNetworkHandler &owner, int32_t head, uint64_t runtimeId) {
    if (runtimeId != 0) {
        if (head == 0) {
            // the game only frees the first side head of the main target
            if (mHeadTargets[1] == runtimeId)
                _setHeadTarget(owner, 1, 0);
        } else {
            // a side head never takes a target a head already has
            for (uint64_t taken: mHeadTargets) {
                if (taken == runtimeId)
                    return;
            }
        }
    }

    if (mHeadTargets[head] == runtimeId)
        return;
    mHeadTargets[head] = runtimeId;
    const Actor *actor = runtimeId == 0 ? nullptr : MobActor::findActor(owner, runtimeId);
    mHeadTargetUniqueIds[head] = actor == nullptr ? -1 : actor->getUniqueId();
    _sendData(owner, FIRST_HEAD_TARGET_DATA_ID + head);
}

void WitherActor::_breakBlocks(ServerNetworkHandler &owner, Level &level, const AxisAlignedBB &area, int32_t inflate,
                               bool charging) {
    if (!level.getGameRules().getBool(GRIEFING_RULE))
        return;

    bool destroyed = false;
    for (int32_t x = (int32_t) area.mMinX - inflate; x <= (int32_t) area.mMaxX + inflate; ++x) {
        for (int32_t z = (int32_t) area.mMinZ - inflate; z <= (int32_t) area.mMaxZ + inflate; ++z) {
            for (int32_t y = (int32_t) area.mMinY - inflate; y <= (int32_t) area.mMaxY + inflate; ++y) {
                const BlockState *state = level.peekBlockPtr(x, y, z);
                if (state == nullptr || state->mName == BlockNames::AIR)
                    continue;
                const Block *block = VanillaBlocks::fromState(*state);
                if (block != nullptr && block->resistsWither(charging))
                    continue;

                const BlockState broken = *state;
                BlockActionHandler::destroyBlock(owner, level, Vector3i(x, y, z), broken, true, ItemStack::air(),
                                                 this);
                destroyed = true;
            }
        }
    }

    if (destroyed) {
        const Vector3f center(area.mMinX + (area.mMaxX - area.mMinX) * 0.5f,
                              area.mMinY + (area.mMaxY - area.mMinY) * 0.5f,
                              area.mMinZ + (area.mMaxZ - area.mMinZ) * 0.5f);
        owner.playLevelSound(level, "break.block", center, IDENTIFIER);
    }
}

bool WitherActor::senseDamage(ServerNetworkHandler &owner, float &amount, const ActorDamageSource &source) {
    if (DamageCause::matches("fall", source) || DamageCause::matches("drowning", source))
        return false;

    Actor *responsible = responsibleOf(owner, source);
    if (dynamic_cast<const WitherActor *>(responsible) != nullptr && !DamageCause::matches("self_destruct", source))
        return false;

    // any mob or player hitting it while it spawns is pushed back
    const bool mob = responsible != nullptr && (responsible->isPlayer() || dynamic_cast<MobActor *>(responsible));
    if (DamageCause::matches("entity_attack", source) && mSpawningFrames > 0 && mob) {
        // the game feeds the height difference in place of the z one
        const Vector3f position = getPosition();
        const Vector3f other = aimPosition(*responsible);
        owner.knockBack(*responsible, other.x - position.x, other.y - position.y, 0.4f, 0.4f);
    }

    if (mInvulnerableTicks > 0 && !DamageCause::matches("void", source))
        return false;

    // below half health its armor stops arrows; tridents and other projectiles still hurt it
    const bool arrow = dynamic_cast<ArrowActor *>(source.mDamager) != nullptr
                       && dynamic_cast<ThrownTridentActor *>(source.mDamager) == nullptr;
    if (!mAirAttack && arrow && (source.mProjectile || DamageCause::matches("projectile", source)))
        return false;

    // all of this before the hit is weighed, as the game does: a hit its damage sensor then refuses still counts
    if (responsible != nullptr && responsible->isPlayer())
        _addDamage(responsible, amount);

    WitherSkullActor *skull = dynamic_cast<WitherSkullActor *>(source.mDamager);
    if (skull != nullptr && !skull->isDangerous())
        mFrozenTicks = FROZEN_TICKS;

    if (mBlockBreakTicks <= 0)
        mBlockBreakTicks = 20;
    for (int32_t &updates: mIdleHeadUpdates)
        updates += 3;

    return HostileActor::senseDamage(owner, amount, source);
}

void WitherActor::onDamaged(ServerNetworkHandler &owner, Actor *attacker) {
    HostileActor::onDamaged(owner, attacker);
    const int32_t health = (int32_t) getHealth();

    // every sixth of its health lost halves the fire rate
    if (owner.getProperties().getDifficulty() != Difficulty::Easy) {
        const int32_t step = mLastHealthStep - mHealthStep;
        if (health < step) {
            mFireRate = (int32_t) ((float) mFireRate * 0.5f);
            mLastHealthStep = step;
        }
    }

    if (health <= mPhaseHealth && mPhase > 0) {
        mPhase--;
        _setAirAttack(owner, false);
        mHalfHealthExplosionPending = true;
        mAerialAttackArmed = false;
        mFireRate = 20;
        mAerialAttackInterval *= 2;
    }

    if (attacker == nullptr)
        return;

    Actor *target = getTarget(owner);
    if (attacker == target
        && (!canSee(owner, *attacker) || distanceSquaredTo(*attacker) > mHeadRange * mHeadRange))
        mAerialAttackTicks = 1;
}

void WitherActor::_addDamage(const Actor *attacker, float amount) {
    mDamageDealt[attacker->getRuntimeId()] += (int32_t) amount;
}

ServerPlayer *WitherActor::findHighestDamagePlayer(ServerNetworkHandler &owner) const {
    ServerPlayer *best = nullptr;
    int32_t bestDamage = 0;
    for (const auto &entry: mDamageDealt) {
        ServerPlayer *player = MobActor::findPlayer(owner, entry.first);
        if (player == nullptr || entry.second <= bestDamage || !canTarget(*player)
            || distanceSquaredTo(*player) > FOLLOW_RANGE * FOLLOW_RANGE)
            continue;

        best = player;
        bestDamage = entry.second;
    }
    return best;
}

bool WitherActor::canBeAffected(MobEffectId id) const {
    return mAirAttack && (id == MobEffectId::InstantHealth || id == MobEffectId::InstantDamage);
}

void WitherActor::kill(ServerNetworkHandler &owner, ServerPlayer *source, int32_t lootingLevel) {
    if (isDead())
        return;

    mKillerRuntimeId = source == nullptr ? 0 : source->getRuntimeId();
    mKillerLooting = lootingLevel;
    mCharging = false;
    mFrozenTicks = 0;
    mChargeWindupTicks = 0;
    owner.playLevelSound(owner.getLevelFor(*this), "death.min.volume", getPosition(), IDENTIFIER);
    _sendBossKilled(owner);
    HostileActor::kill(owner, source, lootingLevel);
}

void WitherActor::tickDeath(ServerNetworkHandler &owner) {
    HostileActor::tickDeath(owner);
    Level &level = owner.getLevelFor(*this);
    const int32_t frame = getDeathTicks() - 1;

    if (mBossPercent != 0.0f) {
        mBossPercent = 0.0f;
        for (auto &entry: owner.getPlayers()) {
            if (mBossViewers.count(entry.second.getUniqueId()) != 0)
                _sendBossEvent(owner, entry.second, BOSS_ACTION_UPDATE_PERCENT);
        }
    }

    if (frame < DEATH_FRAMES) {
        // the armor flashes, faster and faster
        if (frame % mDeathFlashInterval == 0) {
            _setAirAttack(owner, !mAirAttack);
            if (mDeathFlashInterval >= 2)
                mDeathFlashInterval--;
        }
        if (frame == 80)
            owner.playLevelSound(level, "death", getPosition(), IDENTIFIER);
        else if (frame == 140)
            owner.playLevelSound(level, "death.mid.volume", getPosition(), IDENTIFIER);
        _setInvulnerableTicks(owner, DEATH_FRAMES - frame);
        return;
    }

    _explode(owner, level, getPosition());
    dropDeathRewards(owner, mKillerRuntimeId == 0 ? nullptr : MobActor::findPlayer(owner, mKillerRuntimeId),
                     mKillerLooting);
    _clearBossBar(owner);
}

void WitherActor::_setInvulnerableTicks(ServerNetworkHandler &owner, int32_t ticks) {
    if (mInvulnerableTicks == ticks)
        return;
    mInvulnerableTicks = ticks;
    _sendData(owner, INVULNERABLE_TICKS_DATA_ID);
}

void WitherActor::_setAirAttack(ServerNetworkHandler &owner, bool airAttack) {
    if (mAirAttack == airAttack)
        return;
    mAirAttack = airAttack;
    _sendData(owner, AIR_ATTACK_DATA_ID);
}

void WitherActor::_appendData(EntityDataMap &metadata, int32_t id) const {
    EntityDataEntry entry;
    entry.mId = id;
    if (id == INVULNERABLE_TICKS_DATA_ID) {
        entry.mFormat = EntityDataFormat::Int;
        entry.mIntValue = mInvulnerableTicks;
    } else if (id == AIR_ATTACK_DATA_ID) {
        entry.mFormat = EntityDataFormat::Short;
        entry.mShortValue = mAirAttack ? 1 : 0;
    } else if (id == SPAWNING_FRAMES_DATA_ID) {
        entry.mFormat = EntityDataFormat::Int;
        entry.mIntValue = mSpawningFrames;
    } else {
        // the heads' targets go out as unique ids, -1 for none
        entry.mFormat = EntityDataFormat::Long;
        entry.mLongValue = mHeadTargetUniqueIds[(size_t) (id - FIRST_HEAD_TARGET_DATA_ID)];
    }
    metadata.mEntries.push_back(entry);
}

void WitherActor::_sendData(ServerNetworkHandler &owner, int32_t id) const {
    EntityDataMap metadata;
    _appendData(metadata, id);
    owner.sendActorMetadata(const_cast<WitherActor &>(*this), metadata);
}

void WitherActor::fillSpawnMetadata(EntityDataMap &metadata) const {
    HostileActor::fillSpawnMetadata(metadata);
    _appendData(metadata, INVULNERABLE_TICKS_DATA_ID);
    for (int32_t head = 0; head < HEAD_COUNT; ++head)
        _appendData(metadata, FIRST_HEAD_TARGET_DATA_ID + head);
    _appendData(metadata, AIR_ATTACK_DATA_ID);
    _appendData(metadata, SPAWNING_FRAMES_DATA_ID);
}

Tag WitherActor::saveNbt() const {
    Tag data = HostileActor::saveNbt();
    data.putInt(TAG_INVULNERABLE, mInvulnerableTicks);
    data.putByte(TAG_AIR_ATTACK, mAirAttack ? 1 : 0);
    data.putInt(TAG_SHIELD_HEALTH, mShieldHealth);
    data.putInt(TAG_PHASE, mPhase);
    data.putInt(TAG_SPAWNING_FRAMES, mSpawningFrames);
    data.putInt(TAG_MAX_HEALTH, mSavedMaxHealth);
    data.putInt(TAG_LAST_HEALTH_INTERVAL, mLastHealthStep);
    data.putInt(TAG_FIRE_RATE, mFireRate);
    return data;
}

void WitherActor::loadNbt(const Tag &data) {
    HostileActor::loadNbt(data);
    mInvulnerableTicks = data.getInt(TAG_INVULNERABLE, 0);
    mAirAttack = data.getByte(TAG_AIR_ATTACK, 0) != 0;
    mShieldHealth = data.getInt(TAG_SHIELD_HEALTH, 0);
    mPhase = data.getInt(TAG_PHASE, 0);
    mSpawningFrames = data.getInt(TAG_SPAWNING_FRAMES, 0);
    mSavedMaxHealth = data.getInt(TAG_MAX_HEALTH, 0);
    mLastHealthStep = data.getInt(TAG_LAST_HEALTH_INTERVAL, 0);
    mFireRate = data.getInt(TAG_FIRE_RATE, 0);
    mSpawnPending = false;
    mHealthStepsReady = false;
}

void WitherActor::_sendBossEvent(ServerNetworkHandler &owner, ServerPlayer &player, int32_t action) {
    BossEventPacket packet;
    packet.mBossUniqueActorId = getUniqueId();
    packet.mAction = (BossEventPacket::Action) action;
    packet.mTitle = getName();
    packet.mHealthPercentage = mBossPercent;
    packet.mColor = BOSS_COLOR_RED;
    packet.mOverlay = 0;
    owner.sendPacketTo(player.getNetworkIdentifier(), packet);
}

void WitherActor::_tickBossBar(ServerNetworkHandler &owner) {
    const json::Value *boss = getComponent(BOSS_COMPONENT);
    const json::Value *range = boss == nullptr ? nullptr : boss->get("hud_range");
    const float hudRange = range == nullptr ? BOSS_HUD_RANGE : (float) range->number(BOSS_HUD_RANGE);

    const float percent = isDead() ? 0.0f : clampf(getHealth() / std::max(getMaxHealth(), 1.0f), 0.0f, 1.0f);
    const bool changed = percent != mBossPercent;
    mBossPercent = percent;

    const Vector3f position = getPosition();
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        const bool known = player.getVisibleActors().count(getRuntimeId()) != 0;
        const bool inRange = known && distanceSquared(player.getPosition(), position) <= hudRange * hudRange;
        const bool shown = mBossViewers.count(player.getUniqueId()) != 0;

        if (inRange && !shown) {
            _sendBossEvent(owner, player, BOSS_ACTION_CREATE);
            mBossViewers.insert(player.getUniqueId());
        } else if (!inRange && shown) {
            _sendBossEvent(owner, player, BOSS_ACTION_REMOVE);
            mBossViewers.erase(player.getUniqueId());
        } else if (inRange && changed) {
            _sendBossEvent(owner, player, BOSS_ACTION_UPDATE_PERCENT);
        }
    }
}

void WitherActor::_clearBossBar(ServerNetworkHandler &owner) {
    for (auto &entry: owner.getPlayers()) {
        if (mBossViewers.count(entry.second.getUniqueId()) != 0)
            _sendBossEvent(owner, entry.second, BOSS_ACTION_REMOVE);
    }
    mBossViewers.clear();
}

bool WitherActor::isWitherDamage(ServerNetworkHandler &owner, const ActorDamageSource &source) {
    if (dynamic_cast<const WitherSkullActor *>(source.mDamager) != nullptr)
        return true;
    return dynamic_cast<const WitherActor *>(responsibleOf(owner, source)) != nullptr;
}

void WitherActor::leaveWitherRose(ServerNetworkHandler &owner, const Actor &victim) {
    Level &level = owner.getLevelFor(victim);
    const Vector3f position = aimPosition(victim);
    const Vector3i feet((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                        (int32_t) std::floor(position.z));

    const BlockState rose = VanillaBlocks::WITHER_ROSE().toBlockState();
    const Block *roseBlock = VanillaBlocks::fromIdentifier(BlockNames::WITHER_ROSE);
    const BlockState *current = level.peekBlockPtr(feet.x, feet.y, feet.z);
    if (current != nullptr && current->mName == BlockNames::AIR && roseBlock != nullptr
        && roseBlock->canSurvive(level, feet, rose) && level.getGameRules().getBool(GRIEFING_RULE)) {
        level.setBlock(feet, rose, true);
        return;
    }

    owner.spawnItemActor(level, BlockNames::WITHER_ROSE, 1, victim.getPosition());
}

namespace {
    // the game's direction order: down, up, north, south, west, east
    const Vector3i DIRECTIONS[6] = {Vector3i(0, -1, 0), Vector3i(0, 1, 0),  Vector3i(0, 0, -1),
                                    Vector3i(0, 0, 1),  Vector3i(-1, 0, 0), Vector3i(1, 0, 0)};

    // the rows top to bottom: ^ a wither skeleton skull, # soul sand or soul soil, ~ air
    const char *const SUMMON_ROWS[] = {"^^^", "###", "~#~"};
    // what a dispenser checks before placing the skull: the soul blocks alone
    const char *const DISPENSER_ROWS[] = {"###", "~#~"};
    const int32_t ROW_LENGTH = 3;

    struct PatternMatch {
        Vector3i mOrigin;
        int32_t mFinger = 0;
        int32_t mThumb = 0;
    };

    bool cellMatches(Level &level, char cell, const Vector3i &at) {
        const BlockState *state = level.peekBlockPtr(at.x, at.y, at.z);
        if (state == nullptr)
            return false;
        if (cell == '^')
            return state->mName == BlockNames::WITHER_SKELETON_SKULL;
        if (cell == '#')
            return state->mName == BlockNames::SOUL_SAND || state->mName == BlockNames::SOUL_SOIL;
        return state->mName == BlockNames::AIR;
    }

    // rows run along the thumb, columns along finger x thumb, from the origin cell (row 0, column 0)
    Vector3i cellAt(const PatternMatch &match, int32_t row, int32_t column) {
        const Vector3i &finger = DIRECTIONS[match.mFinger];
        const Vector3i &thumb = DIRECTIONS[match.mThumb];
        const Vector3i palm(finger.y * thumb.z - finger.z * thumb.y, finger.z * thumb.x - finger.x * thumb.z,
                            finger.x * thumb.y - finger.y * thumb.x);
        return Vector3i(match.mOrigin.x + thumb.x * row + palm.x * column,
                        match.mOrigin.y + thumb.y * row + palm.y * column,
                        match.mOrigin.z + thumb.z * row + palm.z * column);
    }

    // origins from position - 3 to position on each axis, x first; then every finger and thumb on another axis,
    // in the game's order
    bool findPattern(Level &level, const Vector3i &position, const char *const *rows, int32_t rowCount,
                     PatternMatch &result) {
        const int32_t reach = std::max(rowCount, ROW_LENGTH);
        for (int32_t x = position.x - reach; x <= position.x; ++x) {
            for (int32_t y = position.y - reach; y <= position.y; ++y) {
                for (int32_t z = position.z - reach; z <= position.z; ++z) {
                    for (int32_t finger = 0; finger < 6; ++finger) {
                        for (int32_t thumb = 0; thumb < 6; ++thumb) {
                            if ((finger >> 1) == (thumb >> 1))
                                continue;

                            const PatternMatch candidate{Vector3i(x, y, z), finger, thumb};
                            bool matched = true;
                            for (int32_t row = 0; row < rowCount && matched; ++row) {
                                for (int32_t column = 0; column < ROW_LENGTH && matched; ++column)
                                    matched = cellMatches(level, rows[row][column], cellAt(candidate, row, column));
                            }
                            if (matched) {
                                result = candidate;
                                return true;
                            }
                        }
                    }
                }
            }
        }
        return false;
    }
}

bool WitherActor::canDispenseSkull(Level &level, const Vector3i &position) {
    const BlockState *state = level.peekBlockPtr(position.x, position.y, position.z);
    PatternMatch match;
    return state != nullptr && state->mName == BlockNames::AIR
           && findPattern(level, position, DISPENSER_ROWS, 2, match);
}

bool WitherActor::trySummon(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
    if (position.y < level.getMinY() + 2 || owner.getProperties().getDifficulty() == Difficulty::Peaceful)
        return false;

    PatternMatch match;
    if (!findPattern(level, position, SUMMON_ROWS, 3, match))
        return false;

    // the game centres the spawn on the origin in world axes, whatever way the pattern faces
    const Vector3f spawn((float) match.mOrigin.x + 1.0f + 0.5f, (float) match.mOrigin.y - 2.0f + 0.05f,
                         (float) match.mOrigin.z + 1.0f + 0.5f);
    ServerActor *spawned = owner.spawnActor(level, IDENTIFIER, spawn);
    WitherActor *wither = dynamic_cast<WitherActor *>(spawned);
    if (wither == nullptr)
        return false;

    // a pattern facing north or south turns the wither a quarter
    if (match.mFinger == 2 || match.mFinger == 3) {
        Vector3f rotation = wither->getRotation();
        rotation.y += 90.0f;
        wither->setRotation(rotation);
    }

    const BlockState air = VanillaBlocks::AIR().toBlockState();
    for (int32_t row = 0; row < 3; ++row) {
        for (int32_t column = 0; column < ROW_LENGTH; ++column)
            level.setBlock(cellAt(match, row, column), air, true);
    }

    wither->_awardSpawnAchievement(owner);

    std::mt19937 random(std::random_device{}());
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    for (int32_t i = 0; i < SUMMON_PARTICLE_COUNT; ++i) {
        const float x = spawn.x + unit(random);
        const float y = spawn.y + unit(random) * 3.9f - 2.0f;
        const float z = spawn.z + unit(random);
        owner.broadcastLevelEvent(level, PARTICLE_EVENT, Vector3f(x, y, z), SUMMON_PARTICLE);
    }
    return true;
}

void WitherActor::_awardSpawnAchievement(ServerNetworkHandler &owner) {
    const Vector3f position = getPosition();
    owner.forEachPlayerNear(position, ACHIEVEMENT_RANGE_Z, [&](ServerPlayer &player) {
        const Vector3f other = aimPosition(player);
        if (!player.isSpawned() || !player.sharesLevelWith(*this) || std::fabs(other.x - position.x) > ACHIEVEMENT_RANGE
            || std::fabs(other.y - position.y) > ACHIEVEMENT_RANGE
            || std::fabs(other.z - position.z) > ACHIEVEMENT_RANGE_Z)
            return;

        LegacyTelemetryEventPacket packet;
        packet.mUniqueActorId = player.getUniqueId();
        packet.mUsePlayerId = true;
        packet.mEventData.mType = EventDataType::AchievementAwarded;
        packet.mEventData.mAchievementId = SPAWN_THE_WITHER_ACHIEVEMENT;
        owner.sendPacketTo(player.getNetworkIdentifier(), packet);
    });
}

void WitherActor::_sendBossKilled(ServerNetworkHandler &owner) {
    for (const auto &entry: mDamageDealt) {
        ServerPlayer *player = MobActor::findPlayer(owner, entry.first);
        if (player == nullptr)
            continue;

        LegacyTelemetryEventPacket packet;
        packet.mUniqueActorId = player->getUniqueId();
        packet.mUsePlayerId = true;
        packet.mEventData.mType = EventDataType::BossKilled;
        packet.mEventData.mBossUniqueActorId = getUniqueId();
        packet.mEventData.mPlayerPartySize = (int32_t) mDamageDealt.size();
        packet.mEventData.mBossActorType = WITHER_ACTOR_TYPE;
        owner.sendPacketTo(player->getNetworkIdentifier(), packet);
    }
}
