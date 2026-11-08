#include "World/Level/Block/FenceBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/FenceHelpers.h"

FALCON_REGISTER_BLOCK(FenceBlock, 450);

bool FenceBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_fence");
}

bool FenceBlock::getCollisionShape(const BlockState &state, const Actor *actor, AxisAlignedBB &shape) const {
    (void) state;
    (void) actor;

    shape = FenceHelpers::postShape();
    return true;
}
