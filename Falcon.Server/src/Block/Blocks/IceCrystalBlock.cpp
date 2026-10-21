#include "Block/Blocks/IceCrystalBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/Blocks/PlacementHelpers.h"
#include "Block/Components/PlacementOrientation.h"
#include "Level/Level.h"

FALCON_REGISTER_BLOCK(IceCrystalBlock, 330);

bool IceCrystalBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:ice_crystal";
}

bool IceCrystalBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    if (blockFace < 0 || blockFace > 5)
        return false;
    const Vector3i support = PlacementOrientation::relativePosition(position, blockFace ^ 1);
    const BlockState *state = level.peekBlockPtr(support.x, support.y, support.z);
    if (state == nullptr)
        return false;
    return PlacementHelpers::hasSolidFace(*state, blockFace);
}

bool IceCrystalBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    const int face = PlacementOrientation::faceFromName(state.mStates.getString("minecraft:block_face", "down"));
    const Vector3i support = PlacementOrientation::relativePosition(position, face ^ 1);
    if (level.peekBlockPtr(support.x, support.y, support.z) == nullptr)
        return true;
    return canPlaceAt(level, position, face);
}
