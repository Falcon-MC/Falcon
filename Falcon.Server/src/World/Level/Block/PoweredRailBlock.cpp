#include "World/Level/Block/PoweredRailBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

// before BaseRailBlock, which matches every rail
FALCON_REGISTER_BLOCK(PoweredRailBlock, 139);

bool PoweredRailBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:golden_rail" || identifier == "minecraft:activator_rail";
}
