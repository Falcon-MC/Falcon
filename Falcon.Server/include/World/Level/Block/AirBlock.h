#pragma once

#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockTypeIds.h"
#include "World/Level/Block/BlockNames.h"

class AirBlock final : public Block {
public:
    AirBlock() : Block(BlockTypeIds::AIR, "minecraft:air", "Air") {}

    static bool matches(const std::string &identifier) {
        return identifier == BlockNames::AIR;
    }
};
