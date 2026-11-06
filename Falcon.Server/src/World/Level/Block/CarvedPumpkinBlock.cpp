#include "World/Level/Block/CarvedPumpkinBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(CarvedPumpkinBlock, 205);

bool CarvedPumpkinBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:carved_pumpkin";
}
