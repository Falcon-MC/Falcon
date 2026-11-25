#include "World/Actor/Projectile/ProjectileActor.h"

#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/Hostile/BlazeActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Level.h"
#include "World/Level/WindBurst.h"
#include "Network/ServerNetworkHandler.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Packets/MovePlayerPacket.h"
#include "Core/Math/VectorMath.h"

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
    const float WIND_CHARGE_FACE_OFFSET = 0.25f;
    const char *const BREEZE_WIND_CHARGE = "minecraft:breeze_wind_charge_projectile";
    const char *const BREEZE_WIND_CHARGE_PARTICLE = "minecraft:breeze_wind_explosion_emitter";
    const char *const BREEZE_WIND_CHARGE_SOUND = "breeze_wind_charge.burst";
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

    // a harmless throwable still hurts the player for nothing and pushes, even into a raised shield
    void strikePlayer(ServerNetworkHandler &owner, ServerActor &projectile, const Vector3f &hitPosition,
                      ServerPlayer &player, float force) {
        Actor *shooter = owner.getEntity(projectile.getOwnerUniqueId());
        const Vector3f motion = projectile.getMotion();
        const Vector3f origin(hitPosition.x - motion.x, hitPosition.y - motion.y, hitPosition.z - motion.z);

        ActorDamageSource source = ActorDamageSource::environment("death.attack.arrow", player.getName());
        source.mDeathMessageParameters.push_back(shooter == nullptr ? std::string() : shooter->getName());
        source.mAttacker = shooter;
        source.fromOrigin(origin).asProjectile().withDamager(&projectile);
        if (owner.hurt(player, 0.0f, source) != DamageResult::Ignored)
            knockBackFrom(owner, player, hitPosition, force);
    }
}

bool SnowballActor::onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) {
    if (hitPlayer != nullptr)
        strikePlayer(owner, *this, hitPosition, *hitPlayer, SNOWBALL_KNOCKBACK);

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
    if (hitPlayer != nullptr)
        strikePlayer(owner, *this, hitPosition, *hitPlayer, EGG_KNOCKBACK);

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
        EventSignal<ActorTeleportBeforeEvent> &teleport = owner.getEventBus().before().mActorTeleport;
        if (teleport.hasSubscribers()) {
            ActorTeleportBeforeEvent event(shooter, destination, TeleportCause::EnderPearl);
            teleport.emit(event);
            if (event.isCancelled())
                break;
        }
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
    if (hitPlayer != nullptr) {
        _hitTarget(owner, hitPosition, *hitPlayer);
        _burst(owner, getPosition());
        return true;
    }

    // a block hit bursts a quarter block out of the face it struck
    const Vector3f motion = getMotion();
    const float ax = std::fabs(motion.x);
    const float ay = std::fabs(motion.y);
    const float az = std::fabs(motion.z);
    Vector3f out;
    if (ax >= ay && ax >= az)
        out.x = motion.x > 0.0f ? -1.0f : 1.0f;
    else if (ay >= az)
        out.y = motion.y > 0.0f ? -1.0f : 1.0f;
    else
        out.z = motion.z > 0.0f ? -1.0f : 1.0f;
    _burst(owner, Vector3f(hitPosition.x + out.x * WIND_CHARGE_FACE_OFFSET, hitPosition.y + out.y * WIND_CHARGE_FACE_OFFSET,
                           hitPosition.z + out.z * WIND_CHARGE_FACE_OFFSET));
    return true;
}

bool WindChargeActor::onHitActor(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerActor &hitActor) {
    _hitTarget(owner, hitPosition, hitActor);
    _burst(owner, getPosition());
    return true;
}

void WindChargeActor::_burst(ServerNetworkHandler &owner, const Vector3f &center) {
    Level &level = owner.getLevelFor(*this);
    const bool breeze = getIdentifier() == BREEZE_WIND_CHARGE;

    WindBurst::Settings settings;
    const json::Value *definition = EntityDefinitions::find(getIdentifier());
    const json::Value *components = definition == nullptr ? nullptr : definition->get("components");
    const json::Value *explode = components == nullptr ? nullptr : components->get("minecraft:explode");
    if (explode != nullptr)
        settings = WindBurst::fromComponent(*explode);
    settings.mNeedsMobGriefing = breeze;
    WindBurst::burst(owner, level, center, settings, this);

    owner.spawnParticleEffect(level, breeze ? BREEZE_WIND_CHARGE_PARTICLE : WIND_CHARGE_PARTICLE, center);
    if (breeze)
        owner.playLevelSound(level, BREEZE_WIND_CHARGE_SOUND, center);
    else
        owner.playLevelSound(level, LevelSoundEvent::WIND_CHARGE_BURST, center);
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
    bool dangerousSkull(const std::string &identifier) {
        const json::Value *definition = EntityDefinitions::find(identifier);
        const json::Value *components = definition == nullptr ? nullptr : definition->get("components");
        const json::Value *projectile = components == nullptr ? nullptr : components->get("minecraft:projectile");
        const json::Value *dangerous = projectile == nullptr ? nullptr : projectile->get("is_dangerous");
        return dangerous != nullptr && dangerous->boolean(false);
    }
}

WitherSkullActor::WitherSkullActor(uint64_t runtimeId, const std::string &identifier, const ActorSize &size)
        : DefinitionProjectileActor(runtimeId, identifier, size), mDangerous(dangerousSkull(identifier)) {}
