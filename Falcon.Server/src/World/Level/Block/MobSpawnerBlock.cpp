#include "World/Level/Block/MobSpawnerBlock.h"

#include "World/Level/Block/Actor/MobSpawnerBlockActor.h"
#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(MobSpawnerBlock, 200);

bool MobSpawnerBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:mob_spawner";
}

const char *MobSpawnerBlock::getBlockActorId() const {
    return MobSpawnerBlockActor::BLOCK_ACTOR_ID;
}
