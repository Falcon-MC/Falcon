#include "World/Level/Block/WallBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/FenceHelpers.h"

FALCON_REGISTER_BLOCK(WallBlock, 460);

bool WallBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_wall");
}

bool WallBlock::getCollisionShape(const BlockState &state, const Actor *actor, AxisAlignedBB &shape) const {
    (void) state;
    (void) actor;

    shape = FenceHelpers::postShape();
    return true;
}
