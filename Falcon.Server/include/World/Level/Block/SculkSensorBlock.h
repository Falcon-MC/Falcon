#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class SculkSensorBlock final : public Block {
public:
    explicit SculkSensorBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    uint8_t getLiquidBlockedFaces(const BlockState &state) const override;
};
