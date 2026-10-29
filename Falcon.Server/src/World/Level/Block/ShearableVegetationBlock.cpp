#include "World/Level/Block/ShearableVegetationBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(ShearableVegetationBlock, 335);

bool ShearableVegetationBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:hanging_roots" || identifier == "minecraft:pale_hanging_moss"
           || identifier == "minecraft:small_dripleaf_block";
}

bool ShearableVegetationBlock::getDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                                        std::vector<BlockDrop> &drops) const {
    (void) fortuneLevel;

    // Small dripleaf ignores Silk Touch; the others accept either tool.
    const bool silkTouchAllowed = state.mName != "minecraft:small_dripleaf_block";
    if (isShears(tool) || (silkTouchAllowed && hasSilkTouch(tool)))
        drops.push_back({state.mName, 1});
    return true;
}
