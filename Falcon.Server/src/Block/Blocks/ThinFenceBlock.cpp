#include "Block/Blocks/ThinFenceBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/BlockIdentifier.h"

FALCON_REGISTER_BLOCK(ThinFenceBlock, 470);

namespace {
    const float THIN_MIN = 7.0f / 16.0f;
    const float THIN_MAX = 9.0f / 16.0f;
}

bool ThinFenceBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_pane") || BlockIdentifier::endsWith(identifier, "_bars");
}

bool ThinFenceBlock::getCollisionShape(const BlockState &state, AxisAlignedBB &shape) const {
    (void) state;

    shape = AxisAlignedBB(THIN_MIN, 0.0f, THIN_MIN, THIN_MAX, 1.0f, THIN_MAX);
    return true;
}
