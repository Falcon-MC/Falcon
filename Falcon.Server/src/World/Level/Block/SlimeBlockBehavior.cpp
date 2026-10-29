#include "World/Level/Block/SlimeBlockBehavior.h"

#include "World/Actor/Actor.h"
#include "World/Actor/ActorFlags.h"

#include <algorithm>

bool SlimeBlockBehavior::onEntityLand(Actor &actor, float downwardVelocity) const {
    if (actor.getFlags().get(ActorFlag::Sneaking))
        return false;

    Vector3f motion = actor.getMotion();
    motion.x = std::clamp(motion.x, -0.2f, 0.2f);
    motion.z = std::clamp(motion.z, -0.2f, 0.2f);
    motion.y = std::clamp(-downwardVelocity, 0.0f, 3.92f);
    actor.setMotion(motion);
    actor.resetFallDistance();
    return true;
}

std::optional<float> SlimeBlockBehavior::getFallDamage(const Actor &actor, float vanillaFallDamage) const {
    (void) vanillaFallDamage;
    if (actor.getFlags().get(ActorFlag::Sneaking))
        return std::nullopt;
    return 0.0f;
}
