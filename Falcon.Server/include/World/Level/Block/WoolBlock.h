#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class WoolBlock final : public Block {
public:
    explicit WoolBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    uint8_t getVibrationProperties() const override;
};
