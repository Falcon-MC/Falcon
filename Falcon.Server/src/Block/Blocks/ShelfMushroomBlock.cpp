#include "Block/Blocks/ShelfMushroomBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(ShelfMushroomBlock, 186);

#include "Block/BlockSupport.h"
#include "Block/Components/PlacementOrientation.h"
#include "Level/Level.h"

bool ShelfMushroomBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:shelf_mushroom";
}

bool ShelfMushroomBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    if (blockFace == PlacementOrientation::FACE_DOWN || blockFace == PlacementOrientation::FACE_UP)
        return false;

    const Vector3i supportPosition = BlockSupport::supportOf(position, blockFace);
    const BlockState support = level.getBlockState(supportPosition.x, supportPosition.y, supportPosition.z);
    return BlockSupport::isAttachable(support, blockFace);
}

bool ShelfMushroomBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    const int facing = PlacementOrientation::faceFromName(
            state.mStates.getString("minecraft:cardinal_direction", ""));
    if (facing < PlacementOrientation::FACE_NORTH)
        return true;

    return canPlaceAt(level, position, facing);
}

BlockState ShelfMushroomBlock::applyPlacementOrientation(const BlockState &state,
                                                         const BlockPlacementContext &context) const {
    BlockState result = Block::applyPlacementOrientation(state, context);
    if (context.mFace < PlacementOrientation::FACE_NORTH)
        return result;

    Tag states = result.mStates;
    states.putString("minecraft:cardinal_direction", PlacementOrientation::cardinalName(context.mFace));
    return BlockState(result.mName, states);
}
