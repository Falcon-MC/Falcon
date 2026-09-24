#include "Block/Blocks/WallBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/BlockIdentifier.h"
#include "Block/Blocks/FenceHelpers.h"

FALCON_REGISTER_BLOCK(WallBlock, 460);

bool WallBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_wall");
}

bool WallBlock::getCollisionShape(const BlockState &state, AxisAlignedBB &shape) const {
    (void) state;

    shape = FenceHelpers::postShape();
    return true;
}
