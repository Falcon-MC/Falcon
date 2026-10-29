#include "World/Level/LevelGen/Nether/Tree/LegacyCrimsonTree.h"

#include "World/Level/Block/VanillaBlocks.h"

BlockState LegacyCrimsonTree::getTrunkBlockState() const {
    return VanillaBlocks::CRIMSON_STEM().toBlockState();
}

BlockState LegacyCrimsonTree::getLeafBlockState() const {
    return VanillaBlocks::NETHER_WART_BLOCK().toBlockState();
}
