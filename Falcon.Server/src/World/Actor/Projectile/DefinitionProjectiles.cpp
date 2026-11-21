#include "World/Actor/Projectile/ProjectileActor.h"

#include "Core/Json/Json.h"
#include "Core/Math/MathConstants.h"
#include "Core/Math/VectorMath.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Server/PropertiesSettings.h"
#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/MobEffect.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/Systems/FireSystem.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "World/Level/Explosion.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
    // the knockback the arrows' impact_damage gives, shared by every projectile with knockback on
    const float IMPACT_KNOCKBACK = 0.3f;
    const float DEFAULT_EXPLOSION_POWER = 3.0f;
    const int32_t TICKS_PER_SECOND = 20;
    // LevelEvent ParticleLegacyEvent: the particle type in the low bits
    const int32_t PARTICLE_EVENT = 0x4000;

    // the child actor's death message, as the game picks it by the projectile's type
    const char *deathMessageOf(const std::string &identifier) {
        if (identifier == "minecraft:small_fireball" || identifier == "minecraft:dragon_fireball")
            return "death.attack.fireball";
        if (identifier == "minecraft:shulker_bullet")
            return "death.attack.bullet";
        if (identifier == "minecraft:llama_spit")
            return "death.attack.spit";
        return "death.attack.arrow";
    }

    bool flag(const json::Value *object, const char *key, bool fallback) {
        const json::Value *value = object == nullptr ? nullptr : object->get(key);
        return value == nullptr ? fallback : value->boolean(fallback);
    }

    // minecraft:projectile on_hit, read once per entity
    struct OnHit {
        bool mDamages = false;
        int32_t mDamageMin = 0;
        int32_t mDamageMax = 0;
        bool mKnockback = false;
        // impact_damage catch_fire: the actor struck burns
        bool mIgnitesTarget = false;
        // on_hit catch_fire: the block struck is set alight
        bool mIgnitesBlock = false;
        bool mIgniteNeedsGriefing = false;
        const json::Value *mEffects = nullptr;
        int32_t mParticle = 0;
        bool mParticleOnActor = false;
        bool mParticleOnOther = false;
        // the minecraft:explode of the groups the definition_event adds
        const json::Value *mExplode = nullptr;
    };

    const json::Value *explodeOf(const json::Value &definition, const json::Value *onHit) {
        const json::Value *definitionEvent = onHit == nullptr ? nullptr : onHit->get("definition_event");
        const json::Value *trigger = definitionEvent == nullptr ? nullptr : definitionEvent->get("event_trigger");
        const json::Value *eventName = trigger == nullptr ? nullptr : trigger->get("event");
        const json::Value *events = definition.get("events");
        const json::Value *groups = definition.get("component_groups");
        if (eventName == nullptr || events == nullptr || groups == nullptr)
            return nullptr;

        const json::Value *event = events->get(eventName->string());
        const json::Value *add = event == nullptr ? nullptr : event->get("add");
        const json::Value *names = add == nullptr ? nullptr : add->get("component_groups");
        if (names == nullptr || !names->isArray())
            return nullptr;

        for (const std::unique_ptr<json::Value> &name: names->mArray) {
            const json::Value *group = groups->get(name->string());
            const json::Value *explode = group == nullptr ? nullptr : group->get("minecraft:explode");
            if (explode != nullptr)
                return explode;
        }
        return nullptr;
    }

    OnHit readOnHit(const std::string &identifier) {
        OnHit result;
        const json::Value *definition = EntityDefinitions::find(identifier);
        const json::Value *components = definition == nullptr ? nullptr : definition->get("components");
        const json::Value *projectile = components == nullptr ? nullptr : components->get("minecraft:projectile");
        const json::Value *onHit = projectile == nullptr ? nullptr : projectile->get("on_hit");
        if (onHit == nullptr)
            return result;

        if (const json::Value *impact = onHit->get("impact_damage")) {
            result.mDamages = true;
            const json::Value *damage = impact->get("damage");
            if (damage != nullptr && damage->isNumber()) {
                result.mDamageMin = result.mDamageMax = damage->integer(0);
            } else if (damage != nullptr) {
                const json::Value *low = damage->get("min");
                const json::Value *high = damage->get("max");
                result.mDamageMin = low == nullptr ? 0 : low->integer(0);
                result.mDamageMax = high == nullptr ? result.mDamageMin : high->integer(result.mDamageMin);
            }
            result.mKnockback = flag(impact, "knockback", true);
            result.mIgnitesTarget = flag(impact, "catch_fire", false);
        }

        if (const json::Value *catchFire = onHit->get("catch_fire")) {
            result.mIgnitesBlock = true;
            result.mIgniteNeedsGriefing = flag(catchFire, "fire_affected_by_griefing", false);
        }

        if (const json::Value *mobEffect = onHit->get("mob_effect")) {
            const json::Value *effects = mobEffect->get("effects");
            if (effects != nullptr && effects->isArray())
                result.mEffects = effects;
        }

        if (const json::Value *particle = onHit->get("particle_on_hit")) {
            const json::Value *type = particle->get("particle_type");
            result.mParticle = type == nullptr ? 0 : ProjectileComponent::particleType(type->string());
            result.mParticleOnActor = flag(particle, "on_entity_hit", false);
            result.mParticleOnOther = flag(particle, "on_other_hit", false);
        }

        result.mExplode = explodeOf(*definition, onHit);
        return result;
    }

    const OnHit &onHitOf(const std::string &identifier) {
        static std::unordered_map<std::string, OnHit> cache;
        auto found = cache.find(identifier);
        if (found == cache.end())
            found = cache.emplace(identifier, readOnHit(identifier)).first;
        return found->second;
    }

    Actor *shooterOf(ServerNetworkHandler &owner, const ServerActor &projectile) {
        for (auto &entry: owner.getPlayers()) {
            if (entry.second.getUniqueId() == projectile.getOwnerUniqueId())
                return &entry.second;
        }
        return owner.getActor(projectile.getOwnerUniqueId());
    }

    // the durations by difficulty an effect of mob_effect lists
    void applyEffects(ServerNetworkHandler &owner, const json::Value &effects, Actor &target) {
        const Difficulty difficulty = owner.getProperties().getDifficulty();
        const char *durationKey = difficulty == Difficulty::Hard     ? "durationhard"
                                  : difficulty == Difficulty::Normal ? "durationnormal"
                                                                     : "durationeasy";
        for (const std::unique_ptr<json::Value> &entry: effects.mArray) {
            const json::Value *name = entry->get("effect");
            MobEffectInstance instance;
            if (name == nullptr || !parseDefinitionMobEffect(name->string(), instance.mId))
                continue;

            const json::Value *duration = entry->get(durationKey);
            if (duration == nullptr)
                duration = entry->get("duration");
            const json::Value *amplifier = entry->get("amplifier");
            instance.mDuration = duration == nullptr ? 0 : duration->integer(0);
            instance.mAmplifier = amplifier == nullptr ? 0 : amplifier->integer(0);
            owner.addEffect(target, instance);
        }
    }

    void burn(ServerNetworkHandler &owner, Actor &target, float seconds) {
        if (target.isFireImmune() || target.hasEffect(MobEffectId::FireResistance))
            return;
        target.setFireTicks((int) (seconds * (float) TICKS_PER_SECOND));
        target.setOnFire(true);
        if (auto *actor = dynamic_cast<ServerActor *>(&target))
            owner.syncActorFlags(*actor);
        else if (auto *player = dynamic_cast<ServerPlayer *>(&target))
            owner._sendEntityData(*player);
    }

    void showParticle(ServerNetworkHandler &owner, Level &level, const Vector3f &position, int32_t particle) {
        LevelEventPacket event;
        event.mEventId = PARTICLE_EVENT | particle;
        event.mPosition = position;
        event.mData = 0;
        BlockActionHandler::broadcastToViewers(owner, level, position, event);
    }
}

void DefinitionProjectileActor::_hitTarget(ServerNetworkHandler &owner, const Vector3f &hitPosition, Actor &target) {
    const OnHit &onHit = onHitOf(getIdentifier());
    Actor *shooter = shooterOf(owner, *this);

    if (onHit.mDamages) {
        static thread_local std::mt19937 random(std::random_device{}());
        const int32_t low = std::min(onHit.mDamageMin, onHit.mDamageMax);
        const int32_t high = std::max(onHit.mDamageMin, onHit.mDamageMax);
        const float damage = (float) std::uniform_int_distribution<int32_t>(low, high)(random);
        const Vector3f motion = getMotion();
        const Vector3f origin(hitPosition.x - motion.x, hitPosition.y - motion.y, hitPosition.z - motion.z);

        ActorDamageSource source = ActorDamageSource::environment(deathMessageOf(getIdentifier()), target.getName());
        source.mDeathMessageParameters.push_back(shooter == nullptr ? std::string() : shooter->getName());
        source.mAttacker = shooter;
        source.fromOrigin(origin).asProjectile().withDamager(this);

        bool dealt = false;
        if (auto *player = dynamic_cast<ServerPlayer *>(&target))
            dealt = owner.hurt(*player, damage, source) == DamageResult::Dealt;
        else if (auto *actor = dynamic_cast<ServerActor *>(&target))
            dealt = owner.damageActor(*actor, damage, source);

        if (onHit.mKnockback && target.isAlive()) {
            const Vector3f position = target.getPosition();
            owner.knockBack(target, position.x - hitPosition.x, position.z - hitPosition.z, IMPACT_KNOCKBACK);
        }
        if (dealt && onHit.mIgnitesTarget)
            burn(owner, target, getProjectileComponent().mOnFireTime);
    }

    if (onHit.mIgnitesBlock && target.isAlive())
        burn(owner, target, getProjectileComponent().mOnFireTime);

    if (onHit.mEffects != nullptr && target.isAlive())
        applyEffects(owner, *onHit.mEffects, target);
}

void DefinitionProjectileActor::_hitAnywhere(ServerNetworkHandler &owner, const Vector3f &hitPosition,
                                             bool struckActor) {
    const OnHit &onHit = onHitOf(getIdentifier());
    Level &level = owner.getLevelFor(*this);

    // the block struck burns from the cell the projectile came through
    if (onHit.mIgnitesBlock && !struckActor
        && (!onHit.mIgniteNeedsGriefing || level.getGameRules().getBool("mobgriefing"))) {
        const Vector3f position = getPosition();
        const Vector3f motion = getMotion();
        const Vector3i before((int32_t) std::floor(position.x - motion.x), (int32_t) std::floor(position.y - motion.y),
                              (int32_t) std::floor(position.z - motion.z));
        FireSystem::ignite(owner, level, before);
    }

    if (onHit.mParticle != 0 && (struckActor ? onHit.mParticleOnActor : onHit.mParticleOnOther))
        showParticle(owner, level, hitPosition, onHit.mParticle);

    const int32_t hitSound = getProjectileComponent().mHitSound;
    if (hitSound != LevelSoundEventIds::UNDEFINED)
        owner.playLevelSound(level, LevelSoundEventIds::nameOf(hitSound), hitPosition);

    if (onHit.mExplode != nullptr)
        holdAfterHit(hitPosition);
}

bool DefinitionProjectileActor::onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition,
                                      ServerPlayer *hitPlayer) {
    if (hitPlayer != nullptr)
        _hitTarget(owner, hitPosition, *hitPlayer);
    _hitAnywhere(owner, hitPosition, hitPlayer != nullptr);
    return true;
}

bool DefinitionProjectileActor::onHitActor(ServerNetworkHandler &owner, const Vector3f &hitPosition,
                                           ServerActor &hitActor) {
    _hitTarget(owner, hitPosition, hitActor);
    _hitAnywhere(owner, hitPosition, true);
    return true;
}

void DefinitionProjectileActor::tickAfterHit(ServerNetworkHandler &owner) {
    const json::Value *explode = onHitOf(getIdentifier()).mExplode;
    if (explode == nullptr)
        return;

    Level &level = owner.getLevelFor(*this);
    const Vector3f hitPosition = getPosition();
    const json::Value *power = explode->get("power");
    Explosion explosion(owner, level, hitPosition,
                        power == nullptr ? DEFAULT_EXPLOSION_POWER : power->number(DEFAULT_EXPLOSION_POWER), this,
                        false);

    const bool griefing = level.getGameRules().getBool("mobgriefing");
    explosion.setIncendiary(flag(explode, "causes_fire", false)
                            && (!flag(explode, "fire_affected_by_griefing", false) || griefing));

    // allow_underwater off: under water the blast leaves the blocks
    const bool underwater = LiquidBlocksFetch::at(level, hitPosition).water;
    if (flag(explode, "breaks_blocks", true) && (!flag(explode, "destroy_affected_by_griefing", false) || griefing)
        && (!underwater || flag(explode, "allow_underwater", false)))
        explosion.explode();
    else
        explosion.explodeWithoutBlocks();
}

namespace {
    // the game's six directions, by their Facing number: down, up, north, south, west, east
    const int32_t DIRECTION_STEPS[6][3] = {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};
    const float HOMING_SPEED = 0.15f;
    const float HOMING_ACCELERATION = 1.025f;
    const float HOMING_TURN = 0.2f;
    const float HOMING_FALL = 0.04f;
    // a target within 2 blocks of the leg's goal is flown at straight
    const float HOMING_NEAR_SQUARED = 4.0f;
    const int32_t HOMING_TRIES = 5;

    std::mt19937 &homingRandom() {
        static thread_local std::mt19937 random(std::random_device{}());
        return random;
    }

    int32_t randomBelow(int32_t bound) {
        return std::uniform_int_distribution<int32_t>(0, bound - 1)(homingRandom());
    }

    int32_t axisOf(int32_t direction) {
        if (direction >= 4)
            return 0;
        return direction <= 1 ? 1 : 2;
    }

    bool isAir(Level &level, int32_t x, int32_t y, int32_t z) {
        return level.getBlockState(x, y, z).mName == "minecraft:air";
    }

    float clampUnit(float value) {
        return std::max(-1.0f, std::min(1.0f, value));
    }
}

Actor *DefinitionProjectileActor::_homingTarget(ServerNetworkHandler &owner) const {
    if (mHomingTarget == -1)
        return nullptr;
    for (auto &entry: owner.getPlayers()) {
        if (entry.second.getUniqueId() == mHomingTarget)
            return entry.second.isSpawned() && entry.second.isAlive() ? &entry.second : nullptr;
    }
    ServerActor *actor = owner.getActor(mHomingTarget);
    return actor != nullptr && actor->isAlive() ? actor : nullptr;
}

// the game's move: the leg's goal is the block of the target's middle, unless the target is gone or a
// creative or spectator player, then the block under the projectile
void DefinitionProjectileActor::_chooseDirection(ServerNetworkHandler &owner, int32_t axis) {
    Level &level = owner.getLevelFor(*this);
    const Vector3f position = getPosition();

    Actor *target = _homingTarget(owner);
    const auto *player = dynamic_cast<ServerPlayer *>(target);
    if (player != nullptr && (player->isCreative() || player->isSpectator()))
        target = nullptr;

    float lift = 0.5f;
    int32_t goalX = (int32_t) std::floor(position.x);
    int32_t goalY = (int32_t) std::floor(position.y) - 1;
    int32_t goalZ = (int32_t) std::floor(position.z);
    if (target != nullptr) {
        const auto *sized = dynamic_cast<const ServerActor *>(target);
        const ActorSize size = sized != nullptr ? sized->getSize()
                                                : ActorSize{MovementPhysics::PLAYER_WIDTH, MovementPhysics::PLAYER_HEIGHT};
        lift = std::sqrt(size.mWidth * size.mWidth + size.mHeight * size.mHeight) * 0.5f;
        const Vector3f at = target->getPosition();
        goalX = (int32_t) std::floor(at.x);
        goalY = (int32_t) std::floor(at.y + lift);
        goalZ = (int32_t) std::floor(at.z);
    }

    float toX = (float) goalX + 0.5f;
    float toY = (float) goalY + lift;
    float toZ = (float) goalZ + 0.5f;
    int32_t direction = randomBelow(6);

    const float gapX = toX - position.x;
    const float gapY = toY - position.y;
    const float gapZ = toZ - position.z;
    if (gapX * gapX + gapY * gapY + gapZ * gapZ >= HOMING_NEAR_SQUARED) {
        const int32_t x = (int32_t) std::floor(position.x);
        const int32_t y = (int32_t) std::floor(position.y);
        const int32_t z = (int32_t) std::floor(position.z);
        std::vector<int32_t> open;
        if (axis != 0) {
            if (x < goalX && isAir(level, x + 1, y, z))
                open.push_back(5);
            else if (x > goalX && isAir(level, x - 1, y, z))
                open.push_back(4);
        }
        if (axis != 1) {
            if (goalY > y && isAir(level, x, y + 1, z))
                open.push_back(1);
            else if (goalY < y && isAir(level, x, y - 1, z))
                open.push_back(0);
        }
        if (axis != 2) {
            if (z < goalZ && isAir(level, x, y, z + 1))
                open.push_back(3);
            else if (z > goalZ && isAir(level, x, y, z - 1))
                open.push_back(2);
        }

        if (open.empty()) {
            for (int32_t tries = HOMING_TRIES; tries > 0; --tries) {
                const int32_t *step = DIRECTION_STEPS[direction];
                if (isAir(level, x + step[0], y + step[1], z + step[2]))
                    break;
                direction = randomBelow(6);
            }
        } else {
            direction = open[(size_t) randomBelow((int32_t) open.size())];
        }

        const int32_t *step = DIRECTION_STEPS[direction];
        toX = position.x + (float) step[0];
        toY = position.y + (float) step[1];
        toZ = position.z + (float) step[2];
    }

    mAxis = axisOf(direction);
    const float dx = toX - position.x;
    const float dy = toY - position.y;
    const float dz = toZ - position.z;
    const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
    mTargetDelta = length == 0.0f ? Vector3f()
                                  : Vector3f(dx / length * HOMING_SPEED, dy / length * HOMING_SPEED,
                                             dz / length * HOMING_SPEED);
    mFlightSteps = 10 + 10 * randomBelow(5);
}

// a homing shot starts with the shooter's motion alone
void DefinitionProjectileActor::launchAt(ServerNetworkHandler &owner, const Actor &shooter, const Actor &target) {
    if (!getProjectileComponent().mHoming)
        return;
    setMotion(shooter.getMotion());
    mHomingTarget = target.getUniqueId();
    _chooseDirection(owner, -1);
}

// toward the leg's goal while the target lives, else falling
bool DefinitionProjectileActor::steer(ServerNetworkHandler &owner, Vector3f &motion) {
    if (!getProjectileComponent().mHoming || mHomingTarget == -1)
        return false;

    if (_homingTarget(owner) != nullptr) {
        mTargetDelta = Vector3f(clampUnit(mTargetDelta.x * HOMING_ACCELERATION),
                                clampUnit(mTargetDelta.y * HOMING_ACCELERATION),
                                clampUnit(mTargetDelta.z * HOMING_ACCELERATION));
        motion.x += (mTargetDelta.x - motion.x) * HOMING_TURN;
        motion.y += (mTargetDelta.y - motion.y) * HOMING_TURN;
        motion.z += (mTargetDelta.z - motion.z) * HOMING_TURN;
    } else {
        motion.y -= HOMING_FALL;
    }
    return true;
}

// a leg ends when its steps run out, the way along its axis is blocked, or the target is reached on it
void DefinitionProjectileActor::afterMove(ServerNetworkHandler &owner) {
    if (!getProjectileComponent().mHoming || mHomingTarget == -1)
        return;
    Actor *target = _homingTarget(owner);
    if (target == nullptr)
        return;

    if (mFlightSteps > 0 && --mFlightSteps == 0)
        _chooseDirection(owner, mAxis);
    if (mAxis == -1)
        return;

    const Vector3f position = getPosition();
    const int32_t x = (int32_t) std::floor(position.x);
    const int32_t y = (int32_t) std::floor(position.y);
    const int32_t z = (int32_t) std::floor(position.z);
    // the game looks the way of the direction numbered as the axis
    const int32_t *step = DIRECTION_STEPS[mAxis];
    Level &level = owner.getLevelFor(*this);
    const BlockState ahead = level.getBlockState(x + step[0], y + step[1], z + step[2]);
    const BlockData *data = BlockDataTable::find(ahead.mName.c_str());
    if (ahead.mName != "minecraft:air" && data != nullptr && data->mSolid) {
        _chooseDirection(owner, mAxis);
        return;
    }

    const Vector3f at = target->getPosition();
    const bool reached = mAxis == 0   ? x == (int32_t) std::floor(at.x)
                         : mAxis == 1 ? y == (int32_t) std::floor(at.y)
                                      : z == (int32_t) std::floor(at.z);
    if (reached)
        _chooseDirection(owner, mAxis);
}

// a reflected homing projectile forgets its target
void DefinitionProjectileActor::reflect(Actor &attacker, const Vector3f &direction) {
    ProjectileActor::reflect(attacker, direction);
    mHomingTarget = -1;
    mAxis = -1;
}

// the game's order: destroy_on_hurt first, then reflect_on_hurt once reflect_immunity seconds have flown
bool ProjectileActor::onMeleeHit(ServerNetworkHandler &owner, Actor &attacker) {
    const ProjectileComponent &settings = getProjectileComponent();
    if (settings.mDestroyOnHurt) {
        if (settings.mCritParticleOnHurt)
            showParticle(owner, owner.getLevelFor(*this), getPosition(), ProjectileComponent::particleType("crit"));
        owner.removeActor(getUniqueId());
        return true;
    }
    if (!settings.mReflectOnHurt || getLifetimeTicks() < (int32_t) std::floor(settings.mReflectImmunity * 20.0f))
        return false;

    // the attacker's view: x is the pitch, y the yaw, in degrees
    const Vector3f rotation = attacker.getRotation();
    const float pitch = rotation.x * MathConstants::PI_F / 180.0f;
    const float yaw = rotation.y * MathConstants::PI_F / 180.0f;
    reflect(attacker, Vector3f(-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch)));
    return true;
}

void ProjectileActor::reflect(Actor &attacker, const Vector3f &direction) {
    setMotion(direction);
    setOwnerUniqueId(attacker.getUniqueId());
}
