#include "World/Level/LevelGen/Nether/Tree/LegacyWarpedTree.h"

#include "World/Level/Block/VanillaBlocks.h"

BlockState LegacyWarpedTree::getTrunkBlockState() const {
    return VanillaBlocks::WARPED_STEM().toBlockState();
}

BlockState LegacyWarpedTree::getLeafBlockState() const {
    return VanillaBlocks::WARPED_WART_BLOCK().toBlockState();
}
