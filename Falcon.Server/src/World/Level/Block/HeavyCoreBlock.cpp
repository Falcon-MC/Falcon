#include "World/Level/Block/HeavyCoreBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/LiquidProperties.h"

FALCON_REGISTER_BLOCK(HeavyCoreBlock, 353);

bool HeavyCoreBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:heavy_core";
}

uint8_t HeavyCoreBlock::getLiquidBlockedFaces(const BlockState &state) const {
    (void) state;
    return (uint8_t) (1 << LiquidFace::DOWN);
}
