#include "World/Level/Block/FlowerPotBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(FlowerPotBlock, 200);

bool FlowerPotBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:flower_pot";
}
