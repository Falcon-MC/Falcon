#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class TrialSpawnerBlock final : public Block {
public:
    explicit TrialSpawnerBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    const char *getBlockActorId() const override;
};
