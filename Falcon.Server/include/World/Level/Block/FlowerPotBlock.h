#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class FlowerPotBlock final : public Block {
public:
    explicit FlowerPotBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    const char *getBlockActorId() const override;
};
