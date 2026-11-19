#include "World/Level/Block/ConduitFrameBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockNames.h"

FALCON_REGISTER_BLOCK(ConduitFrameBlock, 200);

bool ConduitFrameBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::PRISMARINE || identifier == BlockNames::PRISMARINE_BRICKS
           || identifier == BlockNames::DARK_PRISMARINE || identifier == BlockNames::SEA_LANTERN;
}
