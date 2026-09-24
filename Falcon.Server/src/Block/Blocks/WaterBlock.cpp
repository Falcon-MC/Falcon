#include "Block/Blocks/WaterBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(WaterBlock, 324);

bool WaterBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:water" || identifier == "minecraft:flowing_water";
}

BlockState WaterBlock::source() {
    Tag states = Tag::ofCompound();
    states.putInt("liquid_depth", 0);
    return BlockState("minecraft:water", states);
}
