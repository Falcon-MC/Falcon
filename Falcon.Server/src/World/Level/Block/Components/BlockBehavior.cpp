#include "World/Level/Block/Components/BlockBehavior.h"

#include "World/Actor/Actor.h"

bool BlockBehavior::onEntityLand(Actor &actor, float downwardVelocity) const {
    (void) actor;
    (void) downwardVelocity;
    return false;
}

std::optional<float> BlockBehavior::getFallDamage(const Actor &actor, float vanillaFallDamage) const {
    (void) actor;
    (void) vanillaFallDamage;
    return std::nullopt;
}
