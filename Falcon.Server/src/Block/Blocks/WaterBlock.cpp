#include "Block/Blocks/WaterBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(WaterBlock, 324);

bool WaterBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:water" || identifier == "minecraft:flowing_water";
}

bool WaterBlock::isSource(const BlockState &state) {
    return matches(state.mName) && state.mStates.getInt("liquid_depth", 0) == 0;
}

BlockState WaterBlock::source() {
    Tag states = Tag::ofCompound();
    states.putInt("liquid_depth", 0);
    return BlockState("minecraft:water", states);
}
