#include "World/Level/Block/SculkVeinBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(SculkVeinBlock, 329);

#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/PlacementHelpers.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Level.h"

bool SculkVeinBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::SCULK_VEIN;
}

// the face the vein clings to has to be a full face, like when it spreads
bool SculkVeinBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    if (blockFace < 0 || blockFace > 5)
        return false;
    const Vector3i support = PlacementOrientation::relativePosition(position, blockFace ^ 1);
    const BlockState *state = level.peekBlockPtr(support.x, support.y, support.z);
    if (state == nullptr)
        return false;
    return PlacementHelpers::hasSolidFace(*state, blockFace);
}
