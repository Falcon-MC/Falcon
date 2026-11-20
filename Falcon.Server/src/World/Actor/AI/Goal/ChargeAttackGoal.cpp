#include "World/Actor/AI/Goal/ChargeAttackGoal.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/AI/Goal/MobAttack.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "World/Actor/Player/ServerPlayer.h"

#include <random>

namespace {
    const float EYE_HEIGHT_RATIO = 0.85f;
    const char *const CHARGE_SOUND = "charge";

    std::mt19937 &chargeRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
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

    void setCharging(ServerNetworkHandler &owner, MobActor &mob, bool charging) {
        if (mob.getFlags().get(ActorFlag::Charging) == charging)
            return;
        mob.getFlags().set(ActorFlag::Charging, charging);
        owner.syncActorFlags(mob);
    }
}

ChargeAttackGoal::ChargeAttackGoal(Settings settings) : mSettings(settings) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool ChargeAttackGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = mob.getTarget(owner);
    if (target == nullptr || mob.getMoveControl().hasWanted())
        return false;
    if (std::uniform_real_distribution<float>(0.0f, 1.0f)(chargeRandom()) >= mSettings.mSuccessRate)
        return false;
    return mob.distanceSquaredTo(*target) > mSettings.mMinDistance * mSettings.mMinDistance;
}

bool ChargeAttackGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = mob.getTarget(owner);
    return mob.getMoveControl().hasWanted() && mob.getFlags().get(ActorFlag::Charging) && target != nullptr
           && target->isAlive();
}

void ChargeAttackGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = mob.getTarget(owner);
    if (target == nullptr)
        return;
    mob.getMoveControl().setWantedPosition(eyesOf(*target), mob.getMovementSpeed() * mSettings.mSpeedMultiplier);
    setCharging(owner, mob, true);
    owner.playLevelSound(owner.getLevelFor(mob), CHARGE_SOUND, mob.getPosition());
}

void ChargeAttackGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    setCharging(owner, mob, false);
}

void ChargeAttackGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    Actor *target = mob.getTarget(owner);
    if (target == nullptr)
        return;
    // touching: the strike ends the charge; close enough still: the aim follows the eyes
    if (MobAttack::isTouching(mob, *target, 0.0f)) {
        MobAttack::hit(owner, mob, *target, mob.getAttackDamage(owner.getProperties().getDifficulty()));
        setCharging(owner, mob, false);
        return;
    }
    if (mob.distanceSquaredTo(*target) < mSettings.mMaxDistance * mSettings.mMaxDistance)
        mob.getMoveControl().setWantedPosition(eyesOf(*target), mob.getMovementSpeed() * mSettings.mSpeedMultiplier);
}
