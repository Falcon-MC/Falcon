#include "World/Level/Block/SculkSensorBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/LiquidProperties.h"

FALCON_REGISTER_BLOCK(SculkSensorBlock, 352);

bool SculkSensorBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:sculk_sensor" || identifier == "minecraft:calibrated_sculk_sensor";
}

uint8_t SculkSensorBlock::getLiquidBlockedFaces(const BlockState &state) const {
    (void) state;
    return (uint8_t) (1 << LiquidFace::DOWN);
}
