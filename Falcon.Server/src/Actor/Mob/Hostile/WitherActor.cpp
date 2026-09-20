#include "Actor/Mob/Hostile/WitherActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(WitherActor, WitherActor::IDENTIFIER);

float WitherActor::resolveMaxHealth(Difficulty difficulty) const {
    switch (difficulty) {
        case Difficulty::Hard:
            return HARD_HEALTH;
        case Difficulty::Normal:
            return NORMAL_HEALTH;
        default:
            return EASY_HEALTH;
    }
}

const std::vector<LootEntry> &WitherActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:nether_star", nullptr, 1, 1, 1.0f}
    };

    return entries;
}
