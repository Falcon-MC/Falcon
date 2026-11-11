#include "World/Level/Block/WallBannerBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(WallBannerBlock, 185);

bool WallBannerBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:wall_banner";
}
