#include "Block/Blocks/FenceBlocks.h"

#include "Block/BlockClassRegistry.h"
#include "Block/BlockIdentifier.h"

FALCON_REGISTER_BLOCK(FenceBlock, 450);
FALCON_REGISTER_BLOCK(WallBlock, 460);
FALCON_REGISTER_BLOCK(ThinFenceBlock, 470);

namespace {
    const float POST_MIN = 0.375f;
    const float POST_MAX = 0.625f;
    const float POST_HEIGHT = 1.5f;
    const float THIN_MIN = 7.0f / 16.0f;
    const float THIN_MAX = 9.0f / 16.0f;

    AxisAlignedBB postShape() {
        return AxisAlignedBB(POST_MIN, 0.0f, POST_MIN, POST_MAX, POST_HEIGHT, POST_MAX);
    }
}

bool FenceBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_fence");
}

bool FenceBlock::getCollisionShape(const BlockState &state, AxisAlignedBB &shape) const {
    (void) state;

    shape = postShape();
    return true;
}

bool WallBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_wall");
}

bool WallBlock::getCollisionShape(const BlockState &state, AxisAlignedBB &shape) const {
    (void) state;

    shape = postShape();
    return true;
}

bool ThinFenceBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_pane") || BlockIdentifier::endsWith(identifier, "_bars");
}

bool ThinFenceBlock::getCollisionShape(const BlockState &state, AxisAlignedBB &shape) const {
    (void) state;

    shape = AxisAlignedBB(THIN_MIN, 0.0f, THIN_MIN, THIN_MAX, 1.0f, THIN_MAX);
    return true;
}
