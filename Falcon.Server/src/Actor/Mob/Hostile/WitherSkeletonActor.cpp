#include "Actor/Mob/Hostile/WitherSkeletonActor.h"

#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(WitherSkeletonActor, WitherSkeletonActor::IDENTIFIER);

const std::vector<LootEntry> &WitherSkeletonActor::getLootEntries() const {
    static const std::vector<LootEntry> entries = {
            {"minecraft:bone", nullptr, 0, 2, 1.0f},
            {"minecraft:coal", nullptr, 1, 1, 0.3333f},
            {"minecraft:stone_sword", nullptr, 1, 1, 0.085f, 0.05f},
            {"minecraft:wither_skeleton_skull", nullptr, 1, 1, 0.025f, 0.02f}
    };

    return entries;
}
