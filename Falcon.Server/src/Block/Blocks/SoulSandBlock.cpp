#include "Block/Blocks/SoulSandBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(SoulSandBlock, 185);

bool SoulSandBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:soul_sand";
}
