#include "Actor/ActorCategory.h"

#include "Actor/ActorClassRegistry.h"
#include "Actor/Mob/MobActor.h"

ActorCategory ActorCategories::of(const std::string &identifier) {
    const MobActor *mob = ActorClassRegistry::getMobPrototype(identifier);
    return mob == nullptr ? ActorCategory::Other : mob->getCategory();
}

bool ActorCategories::isHostile(const std::string &identifier) {
    return of(identifier) == ActorCategory::Hostile;
}

bool ActorCategories::isNeutral(const std::string &identifier) {
    return of(identifier) == ActorCategory::Neutral;
}

bool ActorCategories::isPassive(const std::string &identifier) {
    return of(identifier) == ActorCategory::Passive;
}
