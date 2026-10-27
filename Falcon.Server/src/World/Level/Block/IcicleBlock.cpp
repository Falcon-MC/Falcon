#include "World/Level/Block/IcicleBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/PlacementHelpers.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Level.h"

FALCON_REGISTER_BLOCK(IcicleBlock, 331);

bool IcicleBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:icicle";
}

bool IcicleBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    if (blockFace != PlacementOrientation::FACE_UP && blockFace != PlacementOrientation::FACE_DOWN)
        return false;
    const Vector3i support(position.x, position.y + (blockFace == PlacementOrientation::FACE_DOWN ? 1 : -1), position.z);
    const BlockState *state = level.peekBlockPtr(support.x, support.y, support.z);
    if (state == nullptr)
        return false;
    if (matches(state->mName))
        return state->mStates.getBool("hanging", false) == (blockFace == PlacementOrientation::FACE_DOWN);
    return PlacementHelpers::hasSolidFace(*state, blockFace);
}

bool IcicleBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    const bool hanging = state.mStates.getBool("hanging", false);
    if (level.peekBlockPtr(position.x, position.y + (hanging ? 1 : -1), position.z) == nullptr)
        return true;
    return canPlaceAt(level, position, hanging ? PlacementOrientation::FACE_DOWN : PlacementOrientation::FACE_UP);
}

BlockState IcicleBlock::applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const {
    Tag states = state.mStates;
    states.putByte("hanging", context.mFace == PlacementOrientation::FACE_DOWN ? 1 : 0);
    states.putString("dripstone_thickness", "tip");
    return BlockState(state.mName, states);
}
