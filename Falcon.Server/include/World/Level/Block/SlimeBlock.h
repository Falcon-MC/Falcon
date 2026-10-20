#pragma once

#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockTypeIds.h"

class SlimeBlock final : public Block {
public:
    SlimeBlock() : Block(BlockTypeIds::SLIME, "minecraft:slime", "Slime") {}
};
