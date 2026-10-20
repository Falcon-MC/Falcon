#pragma once

#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockTypeIds.h"

class AirBlock final : public Block {
public:
    AirBlock() : Block(BlockTypeIds::AIR, "minecraft:air", "Air") {}
};
