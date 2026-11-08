#include "World/Level/Block/LavaBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(LavaBlock, 325);

bool LavaBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:lava" || identifier == "minecraft:flowing_lava";
}

bool LavaBlock::isDynamic(const std::string &identifier) {
    return identifier == "minecraft:flowing_lava";
}

int32_t LavaBlock::getTickDelay(DimensionType dimension) const {
    return dimension == DimensionType::Nether ? 10 : 30;
}
