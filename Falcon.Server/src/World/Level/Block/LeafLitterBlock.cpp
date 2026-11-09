#include "World/Level/Block/LeafLitterBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(LeafLitterBlock, 317);

#include "World/Level/Block/Systems/LiquidPhysicsSystem.h"
#include "World/Level/Level.h"

bool LeafLitterBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:leaf_litter";
}

// liquid at its own position fails it too (sub_143C84FE0); the support below comes from the game's rules
bool LeafLitterBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    const LiquidInfo liquid = level.getLiquidInfo(position.x, position.y, position.z);
    if (liquid.water || liquid.lava)
        return false;
    return Block::canSurvive(level, position, state);
}
