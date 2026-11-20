#include "World/Actor/Projectile/ProjectileActor.h"

#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/Hostile/BlazeActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "World/Level/Explosion.h"
#include "World/Level/Level.h"
#include "Server/PropertiesSettings.h"
#include "Network/ServerNetworkHandler.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Packets/MovePlayerPacket.h"
#include "Core/Math/VectorMath.h"
#include "Falcon/Plugin/PluginEvents.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace {
    const float SNOWBALL_KNOCKBACK = 0.3f;
    const float SNOWBALL_BLAZE_DAMAGE = 3.0f;
    const char *SNOWBALL_PARTICLE = "minecraft:snowballpoof";
    // ICE_BALL_BREAK is particle type 105 (v2225)
    const int32_t ICE_BALL_BREAK_PARTICLE_EVENT = 0x4000 | 105;
    const float EGG_KNOCKBACK = 0.2f;
    const int32_t EGG_HATCH_CHANCE = 8;
    const int32_t EGG_QUADRUPLE_HATCH_CHANCE = 32;
    const int32_t EGG_QUADRUPLE_HATCH_COUNT = 4;
    const float EGG_HATCH_HEIGHT = 0.5f;
    const char *HATCHED_ACTOR = "minecraft:chicken";
    const float BABY_SCALE = 0.5f;
    const float ENDER_PEARL_DAMAGE = 5.0f;
    const char *ENDER_PEARL_DEATH_MESSAGE = "death.fell.accident.generic";
    const char *ENDER_PEARL_PARTICLE = "minecraft:endermanpop_emitter";
    const float WIND_CHARGE_RADIUS = 3.5f;
    const float WIND_CHARGE_KNOCKBACK_STRENGTH = 0.2f;
    const float WIND_CHARGE_LIFT = 0.6f;
    const float WIND_CHARGE_BURST_HEIGHT = 1.0f;
    const char *WIND_CHARGE_PARTICLE = "minecraft:wind_explosion_emitter";
    const int32_t EXPERIENCE_BOTTLE_MIN_XP = 3;
    const int32_t EXPERIENCE_BOTTLE_MAX_XP = 11;
    const int32_t EXPERIENCE_BOTTLE_SPLASH_COLOR = 0x00385dc6;
    const float SPLASH_POTION_RADIUS_SQUARED = 16.0f;
    const float SPLASH_POTION_RADIUS = 4.0f;
    const float SPLASH_POTION_MIN_SCALE = 0.25f;
    const char *SPLASH_POTION_PARTICLE = "minecraft:splash_spell_emitter";

    using VectorMath::distanceSquared;

    void knockBackFrom(ServerNetworkHandler &owner, Actor &target, const Vector3f &hitPosition, float force) {
        const Vector3f position = target.getPosition();
        owner.knockBack(target, position.x - hitPosition.x, position.z - hitPosition.z, force);
    }
}

bool SnowballActor::onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) {
    if (hitPlayer != nullptr)
        knockBackFrom(owner, *hitPlayer, hitPosition, SNOWBALL_KNOCKBACK);

    owner.spawnParticleEffect(owner.getLevelFor(*this), SNOWBALL_PARTICLE, hitPosition);
    return true;
}

bool SnowballActor::onHitActor(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerActor &hitActor) {
    const float damage = dynamic_cast<const BlazeActor *>(&hitActor) != nullptr ? SNOWBALL_BLAZE_DAMAGE : 0.0f;
    owner.damageActor(hitActor, damage, nullptr);
    knockBackFrom(owner, hitActor, hitPosition, SNOWBALL_KNOCKBACK);
    owner.spawnParticleEffect(owner.getLevelFor(*this), SNOWBALL_PARTICLE, hitPosition);
    return true;
}

bool IceBallActor::onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) {
    (void) hitPlayer;
    LevelEventPacket particle;
    particle.mEventId = ICE_BALL_BREAK_PARTICLE_EVENT;
    particle.mPosition = hitPosition;
    particle.mData = 0;
    BlockActionHandler::broadcastToViewers(owner, owner.getLevelFor(*this), hitPosition, particle);
    owner.playLevelSound(owner.getLevelFor(*this), "ice_ball.break", hitPosition);
    return true;
}

bool IceBallActor::onHitActor(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerActor &hitActor) {
    (void) hitActor;
    return onHit(owner, hitPosition, nullptr);
}

bool EggActor::onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) {
    (void) hitPlayer;

    _hatchChicks(owner, hitPosition);
    return true;
}

bool EggActor::onHitActor(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerActor &hitActor) {
    owner.damageActor(hitActor, 0.0f, nullptr);
    knockBackFrom(owner, hitActor, hitPosition, EGG_KNOCKBACK);
    return true;
}

void EggActor::_hatchChicks(ServerNetworkHandler &owner, const Vector3f &hitPosition) const {
    static std::mt19937 hatchRandom(std::random_device{}());

    if (std::uniform_int_distribution<int32_t>(0, EGG_HATCH_CHANCE - 1)(hatchRandom) != 0)
        return;

    int32_t chicks = 1;
    if (std::uniform_int_distribution<int32_t>(0, EGG_QUADRUPLE_HATCH_CHANCE - 1)(hatchRandom) == 0)
        chicks = EGG_QUADRUPLE_HATCH_COUNT;

    Level &level = owner.getLevelFor(*this);
    const Vector3f spawnPosition(hitPosition.x, hitPosition.y + EGG_HATCH_HEIGHT, hitPosition.z);

    for (int32_t chick = 0; chick < chicks; ++chick) {
        owner.spawnBabyActor(level, HATCHED_ACTOR, spawnPosition, BABY_SCALE);
    }
}

bool EnderPearlActor::onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) {
    (void) hitPlayer;

    const int64_t ownerId = getOwnerUniqueId();
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &shooter = entry.second;
        if (shooter.getUniqueId() != ownerId)
            continue;

        Vector3f destination = hitPosition;
        if (!PluginEvents::teleport(shooter, destination, "ender_pearl"))
            break;
        shooter.teleport(owner, destination, MovePlayerTeleportationCause::Behavior);
        owner.hurt(shooter, ENDER_PEARL_DAMAGE,
                   ActorDamageSource::environment(ENDER_PEARL_DEATH_MESSAGE, shooter.getName())
                           .withoutArmor()
                           .withoutCooldown());
        break;
    }

    owner.spawnParticleEffect(owner.getLevelFor(*this), ENDER_PEARL_PARTICLE, hitPosition);
    return true;
}

bool WindChargeActor::onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) {
    (void) hitPlayer;

    const float radiusSquared = WIND_CHARGE_RADIUS * WIND_CHARGE_RADIUS;

    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &nearby = entry.second;
        if (!nearby.isSpawned() || !nearby.sharesLevelWith(*this))
            continue;

        if (distanceSquared(nearby.getPosition(), hitPosition) > radiusSquared)
            continue;

        owner.pushFrom(nearby, hitPosition, WIND_CHARGE_KNOCKBACK_STRENGTH, WIND_CHARGE_LIFT);
    }

    for (auto &entry: owner.getActors()) {
        ServerActor &nearby = *entry.second;
        if (!nearby.isAlive() || nearby.isProjectile() || !nearby.sharesLevelWith(*this))
            continue;

        if (distanceSquared(nearby.getPosition(), hitPosition) > radiusSquared)
            continue;

        owner.pushFrom(nearby, hitPosition, WIND_CHARGE_KNOCKBACK_STRENGTH, WIND_CHARGE_LIFT);
    }

    Level &level = owner.getLevelFor(*this);
    const Vector3f burstPosition(hitPosition.x, hitPosition.y + WIND_CHARGE_BURST_HEIGHT, hitPosition.z);
    owner.spawnParticleEffect(level, WIND_CHARGE_PARTICLE, burstPosition);
    owner.playLevelSound(level, LevelSoundEvent::WIND_CHARGE_BURST, burstPosition);
    return true;
}

bool ExperienceBottleActor::onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) {
    (void) hitPlayer;

    static std::mt19937 bottleRandom(0x3A5F19C7u);
    std::uniform_int_distribution<int> amount(EXPERIENCE_BOTTLE_MIN_XP, EXPERIENCE_BOTTLE_MAX_XP);

    Level &level = owner.getLevelFor(*this);
    owner.spawnExperienceOrbs(level, hitPosition, amount(bottleRandom));
    owner.broadcastLevelEvent(level, LevelEventPacket::ParticleSplash, hitPosition, EXPERIENCE_BOTTLE_SPLASH_COLOR);
    owner.playLevelSound(level, LevelSoundEvent::GLASS, hitPosition);
    return true;
}

bool SplashPotionActor::onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) {
    (void) hitPlayer;

    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &nearby = entry.second;
        if (!nearby.isSpawned() || !nearby.sharesLevelWith(*this))
            continue;

        const float distance = distanceSquared(nearby.getPosition(), hitPosition);
        if (distance > SPLASH_POTION_RADIUS_SQUARED)
            continue;

        const float scale = std::max(SPLASH_POTION_MIN_SCALE, 1.0f - std::sqrt(distance) / SPLASH_POTION_RADIUS);
        owner.applyPotionEffects(nearby, getPotionId(), scale);
    }

    for (auto &entry: owner.getActors()) {
        ServerActor &nearby = *entry.second;
        if (&nearby == this || !nearby.isAlive() || nearby.isProjectile() || !nearby.sharesLevelWith(*this))
            continue;

        const float distance = distanceSquared(nearby.getPosition(), hitPosition);
        if (distance > SPLASH_POTION_RADIUS_SQUARED)
            continue;

        const float scale = std::max(SPLASH_POTION_MIN_SCALE, 1.0f - std::sqrt(distance) / SPLASH_POTION_RADIUS);
        owner.applyPotionEffects(nearby, getPotionId(), scale);
    }

    owner.spawnParticleEffect(owner.getLevelFor(*this), SPLASH_POTION_PARTICLE, hitPosition);
    return true;
}

bool LingeringPotionActor::onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) {
    (void) hitPlayer;

    owner.spawnLingeringCloud(owner.getLevelFor(*this), hitPosition, getPotionId());
    return true;
}

namespace {
    const int32_t DRAGON_BREATH_POTION = 23;
    const int32_t DRAGON_BREATH_PARTICLE = 49;
    const float DRAGON_BREATH_RADIUS = 6.0f;
    const int32_t DRAGON_BREATH_DURATION = 120;
    const int32_t DRAGON_BREATH_REAPPLICATION_DELAY = 20;
}

void DragonFireballActor::_burst(ServerNetworkHandler &owner, const Vector3f &hitPosition) {
    Level &level = owner.getLevelFor(*this);
    owner.playLevelSound(level, LevelSoundEvent::EXPLODE, hitPosition);

    ServerNetworkHandler::AreaEffectCloudSettings settings;
    settings.mPotionId = DRAGON_BREATH_POTION;
    settings.mRadius = DRAGON_BREATH_RADIUS;
    settings.mDuration = DRAGON_BREATH_DURATION;
    settings.mReapplicationDelay = DRAGON_BREATH_REAPPLICATION_DELAY;
    settings.mParticle = DRAGON_BREATH_PARTICLE;
    settings.mOwnerUniqueId = getOwnerUniqueId();
    owner.spawnAreaEffectCloud(level, hitPosition, settings);
}

bool DragonFireballActor::onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) {
    (void) hitPlayer;

    _burst(owner, hitPosition);
    return true;
}

bool DragonFireballActor::onHitActor(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerActor &hitActor) {
    (void) hitActor;

    _burst(owner, hitPosition);
    return true;
}

namespace {
    const float SKULL_DEFAULT_EXPLOSION_POWER = 3.0f;

    const json::Value *skullProjectile(const std::string &identifier) {
        const json::Value *definition = EntityDefinitions::find(identifier);
        const json::Value *components = definition == nullptr ? nullptr : definition->get("components");
        return components == nullptr ? nullptr : components->get("minecraft:projectile");
    }

    bool skullFlag(const json::Value *component, const char *key, bool fallback) {
        const json::Value *value = component == nullptr ? nullptr : component->get(key);
        return value == nullptr ? fallback : value->boolean(fallback);
    }

    // the minecraft:explode of the groups the on_hit definition_event adds
    const json::Value *skullExplode(const std::string &identifier) {
        const json::Value *definition = EntityDefinitions::find(identifier);
        const json::Value *projectile = skullProjectile(identifier);
        const json::Value *onHit = projectile == nullptr ? nullptr : projectile->get("on_hit");
        const json::Value *definitionEvent = onHit == nullptr ? nullptr : onHit->get("definition_event");
        const json::Value *trigger = definitionEvent == nullptr ? nullptr : definitionEvent->get("event_trigger");
        const json::Value *eventName = trigger == nullptr ? nullptr : trigger->get("event");
        const json::Value *events = definition == nullptr ? nullptr : definition->get("events");
        const json::Value *groups = definition == nullptr ? nullptr : definition->get("component_groups");
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
}

WitherSkullActor::WitherSkullActor(uint64_t runtimeId, const std::string &identifier, const ActorSize &size)
        : ThrownProjectileActor(runtimeId, identifier, size) {
    mDangerous = skullFlag(skullProjectile(identifier), "is_dangerous", false);
}

void WitherSkullActor::_applyEffects(ServerNetworkHandler &owner, Actor &target) const {
    const json::Value *projectile = skullProjectile(getIdentifier());
    const json::Value *onHit = projectile == nullptr ? nullptr : projectile->get("on_hit");
    const json::Value *mobEffect = onHit == nullptr ? nullptr : onHit->get("mob_effect");
    const json::Value *effects = mobEffect == nullptr ? nullptr : mobEffect->get("effects");
    if (effects == nullptr || !effects->isArray())
        return;

    const Difficulty difficulty = owner.getProperties().getDifficulty();
    const char *durationKey = difficulty == Difficulty::Hard     ? "durationhard"
                              : difficulty == Difficulty::Normal ? "durationnormal"
                                                                 : "durationeasy";
    for (const std::unique_ptr<json::Value> &entry: effects->mArray) {
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

void WitherSkullActor::tickAfterHit(ServerNetworkHandler &owner) {
    const json::Value *explode = skullExplode(getIdentifier());
    if (explode == nullptr)
        return;

    Level &level = owner.getLevelFor(*this);
    const Vector3f hitPosition = getPosition();
    const json::Value *power = explode->get("power");
    Explosion explosion(owner, level, hitPosition,
                        power == nullptr ? SKULL_DEFAULT_EXPLOSION_POWER
                                         : power->number(SKULL_DEFAULT_EXPLOSION_POWER),
                        this, false);
    explosion.setIncendiary(skullFlag(explode, "causes_fire", false));

    const bool griefing = !skullFlag(explode, "destroy_affected_by_griefing", false)
                          || level.getGameRules().getBool("mobgriefing");
    // allow_underwater off: under water the blast leaves the blocks
    const bool underwater = LiquidBlocksFetch::at(level, hitPosition).water;
    if (skullFlag(explode, "breaks_blocks", true) && griefing
        && (!underwater || skullFlag(explode, "allow_underwater", false)))
        explosion.explode();
    else
        explosion.explodeWithoutBlocks();
}

bool WitherSkullActor::onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) {
    if (hitPlayer != nullptr)
        _applyEffects(owner, *hitPlayer);
    if (skullExplode(getIdentifier()) != nullptr)
        holdAfterHit(hitPosition);
    return true;
}

bool WitherSkullActor::onHitActor(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerActor &hitActor) {
    _applyEffects(owner, hitActor);
    if (skullExplode(getIdentifier()) != nullptr)
        holdAfterHit(hitPosition);
    return true;
}
