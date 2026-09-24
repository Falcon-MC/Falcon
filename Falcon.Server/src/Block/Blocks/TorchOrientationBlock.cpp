#include "Block/Blocks/TorchOrientationBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/BlockIdentifier.h"
#include "Block/BlockSupport.h"
#include "Block/Blocks/OrientationHelpers.h"
#include "Block/Components/BlockPlacementComponent.h"
#include "Block/Components/PlacementOrientation.h"
#include "Level/Level.h"

FALCON_REGISTER_BLOCK(TorchOrientationBlock, 170);

bool TorchOrientationBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:torch"
           || BlockIdentifier::startsWith(identifier, "minecraft:colored_torch_")
           || BlockIdentifier::endsWith(identifier, "_torch");
}

BlockState TorchOrientationBlock::applyPlacementOrientation(const BlockState &state,
                                                            const BlockPlacementContext &context) const {
    using namespace PlacementOrientation;

    BlockState result = Block::applyPlacementOrientation(state, context);
    Tag states = result.mStates;
    const int facing = context.mFace >= FACE_NORTH ? context.mFace : FACE_UP;
    OrientationHelpers::setFacingDirection(states, facing);
    return BlockState(result.mName, states);
}

bool TorchOrientationBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    const int face = blockFace >= PlacementOrientation::FACE_NORTH ? blockFace : PlacementOrientation::FACE_UP;
    const Vector3i supportPosition = BlockSupport::supportOf(position, face);
    const BlockState support = level.getBlockState(supportPosition.x, supportPosition.y, supportPosition.z);
    return BlockSupport::isAttachable(support, face);
}

bool TorchOrientationBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    using namespace PlacementOrientation;

    int face = FACE_UP;
    if (state.mStates.contains("torch_facing_direction"))
        face = faceFromTorchFacing(state.mStates.getString("torch_facing_direction", "top"));
    else if (state.mStates.contains("facing_direction"))
        face = state.mStates.getInt("facing_direction", FACE_UP);

    return canPlaceAt(level, position, face);
}
