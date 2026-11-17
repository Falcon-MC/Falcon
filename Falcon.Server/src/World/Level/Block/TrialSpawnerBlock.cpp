#include "World/Level/Block/TrialSpawnerBlock.h"

#include "World/Level/Block/Actor/TrialSpawnerBlockActor.h"
#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(TrialSpawnerBlock, 200);

bool TrialSpawnerBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:trial_spawner";
}

const char *TrialSpawnerBlock::getBlockActorId() const {
    return TrialSpawnerBlockActor::BLOCK_ACTOR_ID;
}
