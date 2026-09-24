#include "Block/Blocks/FenceBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/BlockIdentifier.h"
#include "Block/Blocks/FenceHelpers.h"

FALCON_REGISTER_BLOCK(FenceBlock, 450);

bool FenceBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_fence");
}

bool FenceBlock::getCollisionShape(const BlockState &state, AxisAlignedBB &shape) const {
    (void) state;

    shape = FenceHelpers::postShape();
    return true;
}
