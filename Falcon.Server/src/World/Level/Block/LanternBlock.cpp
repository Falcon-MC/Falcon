#include "World/Level/Block/LanternBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"

FALCON_REGISTER_BLOCK(LanternBlock, 200);

bool LanternBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:lantern" || identifier == "minecraft:soul_lantern"
           || BlockIdentifier::endsWith(identifier, "copper_lantern");
}
