#include "Actor/Projectile/ProjectileActor.h"

#include "Actor/ActorFlags.h"
#include "Actor/Mob/Hostile/BlazeActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Packets/MovePlayerPacket.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace {
    const float SNOWBALL_KNOCKBACK = 0.3f;
    const float SNOWBALL_BLAZE_DAMAGE = 3.0f;
    const char *SNOWBALL_PARTICLE = "minecraft:snowballpoof";
    const float EGG_KNOCKBACK = 0.2f;
    const int32_t EGG_HATCH_CHANCE = 8;
    const int32_t EGG_QUADRUPLE_HATCH_CHANCE = 32;
    const int32_t EGG_QUADRUPLE_HATCH_COUNT = 4;
    const float EGG_HATCH_HEIGHT = 0.5f;
    const char *HATCHED_ACTOR = "minecraft:chicken";
    const int32_t ACTOR_DATA_SCALE = 38;
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

    float distanceSquared(const Vector3f &left, const Vector3f &right) {
        const float dx = left.x - right.x;
        const float dy = left.y - right.y;
        const float dz = left.z - right.z;
        return dx * dx + dy * dy + dz * dz;
    }

    void knockBackFrom(ServerNetworkHandler &owner, Actor &target, const Vector3f &hitPosition, float force) {
        const Vector3f position = target.getPosition();
        owner.knockBack(target, position.x - hitPosition.x, position.z - hitPosition.z, force);
    }

    void pushEntry(EntityDataMap &metadata, int32_t id, int64_t value) {
        EntityDataEntry entry;
        entry.mId = id;
        entry.mFormat = EntityDataFormat::Long;
        entry.mLongValue = value;
        metadata.mEntries.push_back(entry);
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
        ServerActor *hatched = owner.spawnActor(level, HATCHED_ACTOR, spawnPosition);
        if (hatched == nullptr)
            continue;

        hatched->getFlags().set(ActorFlag::Baby, true);

        EntityDataMap metadata;
        pushEntry(metadata, ActorFlags::FLAGS_DATA_ID, hatched->getFlags().getLowBits());
        pushEntry(metadata, ActorFlags::FLAGS_2_DATA_ID, hatched->getFlags().getHighBits());

        EntityDataEntry scale;
        scale.mId = ACTOR_DATA_SCALE;
        scale.mFormat = EntityDataFormat::Float;
        scale.mFloatValue = BABY_SCALE;
        metadata.mEntries.push_back(scale);

        owner.sendActorMetadata(*hatched, metadata);
    }
}

bool EnderPearlActor::onHit(ServerNetworkHandler &owner, const Vector3f &hitPosition, ServerPlayer *hitPlayer) {
    (void) hitPlayer;

    const int64_t ownerId = getOwnerUniqueId();
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &shooter = entry.second;
        if ((int64_t) shooter.getRuntimeId() != ownerId)
            continue;

        shooter.teleport(owner, hitPosition, MovePlayerTeleportationCause::Behavior);
        owner.applyDamage(shooter, ENDER_PEARL_DAMAGE, ENDER_PEARL_DEATH_MESSAGE, {shooter.getName()}, false, false);
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
        if (!nearby.isSpawned() || nearby.getDimension() != getDimension())
            continue;

        if (distanceSquared(nearby.getPosition(), hitPosition) > radiusSquared)
            continue;

        owner.pushFrom(nearby, hitPosition, WIND_CHARGE_KNOCKBACK_STRENGTH, WIND_CHARGE_LIFT);
    }

    for (auto &entry: owner.getActors()) {
        ServerActor &nearby = *entry.second;
        if (!nearby.isAlive() || nearby.isProjectile() || nearby.getDimension() != getDimension())
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
        if (!nearby.isSpawned() || nearby.getDimension() != getDimension())
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
