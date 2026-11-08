#include "World/Level/Block/BannerBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(BannerBlock, 200);

bool BannerBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:standing_banner";
}
