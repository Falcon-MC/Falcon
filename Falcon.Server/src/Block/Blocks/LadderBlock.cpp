#include "Block/Blocks/LadderBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(LadderBlock, 180);

#include "Block/BlockIdentifier.h"
#include "Block/BlockSupport.h"
#include "Block/Blocks/ThinFenceBlock.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Components/PlacementOrientation.h"
#include "Level/Level.h"

bool LadderBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:ladder";
}

bool LadderBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    if (blockFace == PlacementOrientation::FACE_DOWN || blockFace == PlacementOrientation::FACE_UP)
        return false;

    const Vector3i supportPosition = BlockSupport::supportOf(position, blockFace);
    const BlockState support = level.getBlockState(supportPosition.x, supportPosition.y, supportPosition.z);

    if (BlockIdentifier::endsWith(support.mName, "_stained_glass")
        || VanillaBlocks::getAs<ThinFenceBlock>(support.mName) != nullptr
        || BlockIdentifier::endsWith(support.mName, "_leaves") || support.mName == "minecraft:beacon")
        return false;

    return BlockSupport::isAttachable(support, blockFace);
}

bool LadderBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    if (!state.mStates.contains("facing_direction"))
        return true;

    return canPlaceAt(level, position, state.mStates.getInt("facing_direction"));
}
