#include "Actor/Projectile/ProjectileActor.h"

#include "Actor/ActorFlags.h"
#include "Actor/Mob/Hostile/BlazeActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

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
