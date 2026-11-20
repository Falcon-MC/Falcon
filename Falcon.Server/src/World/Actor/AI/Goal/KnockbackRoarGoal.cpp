#include "World/Actor/AI/Goal/KnockbackRoarGoal.h"

#include "Core/Math/AxisAlignedBB.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/DamageCause.h"
#include "World/Actor/Definition/EntityEvents.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Actor/Player/ServerPlayer.h"

#include <utility>
#include <vector>

namespace {
    const char *const ROAR_SOUND = "roar";
    const int32_t ROAR_PARTICLES_EVENT = 2022;
    const char *const ROAR_CAUSE = "entity_explosion";
    // the definition's strengths are scaled to motion
    const float STRENGTH_SCALE = 0.4f;

    void setRoaring(ServerNetworkHandler &owner, MobActor &mob, bool roaring) {
        if (mob.getFlags().get(ActorFlag::Roaring) == roaring)
            return;
        mob.getFlags().set(ActorFlag::Roaring, roaring);
        owner.syncActorFlags(mob);
    }
}

KnockbackRoarGoal::KnockbackRoarGoal(Settings settings) : mSettings(std::move(settings)) {}

bool KnockbackRoarGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    // every call counts toward the cooldown, as in the game
    return mCooldownTicks++ > mSettings.mCooldown;
}

bool KnockbackRoarGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    return mTicks < mSettings.mDuration;
}

void KnockbackRoarGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    setRoaring(owner, mob, true);
    owner.playLevelSound(owner.getLevelFor(mob), ROAR_SOUND, mob.getPosition());
    mCooldownTicks = 0;
    mTicks = 0;
}

void KnockbackRoarGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    setRoaring(owner, mob, false);
    EntityEvents::fireTrigger(owner, mob, mSettings.mOnRoarEnd.get());
}

void KnockbackRoarGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (++mTicks == mSettings.mAttackTime)
        _roar(owner, mob);
}

void KnockbackRoarGoal::_strike(ServerNetworkHandler &owner, MobActor &mob, Actor &target, float sourceX,
                                float sourceZ) {
    if (mSettings.mKnockbackFilters != nullptr
        && !EntityFilter::test(*mSettings.mKnockbackFilters, owner, mob, &target))
        return;

    const Vector3f &position = target.getPosition();
    owner.knockBack(target, position.x - sourceX, position.z - sourceZ,
                    (float) mSettings.mHorizontalStrength * STRENGTH_SCALE, mSettings.mHeightCap);
    if (mSettings.mDamageFilters != nullptr && !EntityFilter::test(*mSettings.mDamageFilters, owner, mob, &target))
        return;

    const ActorDamageSource source =
            ActorDamageSource::attack(DamageCause::findDeathMessageKey(ROAR_CAUSE), target.getName(), mob,
                                      mob.getName(), mob.getPosition())
                    .withCause(ROAR_CAUSE);
    if (ServerPlayer *player = dynamic_cast<ServerPlayer *>(&target))
        owner.hurt(*player, (float) mSettings.mDamage, source);
    else if (ServerActor *actor = dynamic_cast<ServerActor *>(&target))
        owner.damageActor(*actor, (float) mSettings.mDamage, source);
}

void KnockbackRoarGoal::_roar(ServerNetworkHandler &owner, MobActor &mob) {
    const AxisAlignedBB bounds = ActorPushSystem::boundingBoxOf(mob);
    const float range = (float) mSettings.mRange;
    const AxisAlignedBB area = bounds.expand(range, range, range);
    // the game pushes away from the corner of the mob's box, not its centre
    const float sourceX = bounds.mMinX;
    const float sourceZ = bounds.mMinZ;

    std::vector<Actor *> targets;
    owner.forEachActorNear(mob.getPosition(), range + mob.getSize().mWidth, [&](ServerActor &actor) {
        if (&actor != &mob && dynamic_cast<MobActor *>(&actor) != nullptr
            && area.intersectsWith(ActorPushSystem::boundingBoxOf(actor)))
            targets.push_back(&actor);
    });
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (player.isSpawned() && &owner.getLevelFor(player) == &owner.getLevelFor(mob)
            && area.intersectsWith(ActorPushSystem::boundingBoxOf(player)))
            targets.push_back(&player);
    }
    for (Actor *target: targets)
        _strike(owner, mob, *target, sourceX, sourceZ);

    const Vector3f centre((bounds.mMinX + bounds.mMaxX) * 0.5f, (bounds.mMinY + bounds.mMaxY) * 0.5f,
                          (bounds.mMinZ + bounds.mMaxZ) * 0.5f);
    owner.broadcastLevelEvent(owner.getLevelFor(mob), ROAR_PARTICLES_EVENT, centre, 0);
}
