#pragma once

#include "World/Level/Block/Block.h"

#include <string>

// the monster spawner: its block actor (MobSpawnerBlockActor) does the spawning
class MobSpawnerBlock final : public Block {
public:
    explicit MobSpawnerBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    const char *getBlockActorId() const override;
};
