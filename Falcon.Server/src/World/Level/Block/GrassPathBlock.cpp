#include "World/Level/Block/GrassPathBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(GrassPathBlock, 246);

bool GrassPathBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:grass_path";
}

// 15/16 high: the client stands players a sixteenth lower than on a full block
bool GrassPathBlock::getCollisionShape(const BlockState &state, const Actor *actor, AxisAlignedBB &shape) const {
    (void) state;
    (void) actor;
    shape = AxisAlignedBB(0.0f, 0.0f, 0.0f, 1.0f, 0.9375f, 1.0f);
    return true;
}
