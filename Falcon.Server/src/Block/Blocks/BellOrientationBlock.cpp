#include "Block/Blocks/BellOrientationBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/BlockData.h"
#include "Block/Blocks/OrientationHelpers.h"
#include "Block/Components/BlockPlacementComponent.h"
#include "Block/Components/PlacementOrientation.h"
#include "Level/Level.h"

FALCON_REGISTER_BLOCK(BellOrientationBlock, 200);

namespace {
    bool isSolidNeighbour(Level *level, const Vector3i &position) {
        if (level == nullptr)
            return false;

        const BlockState state = level->getBlockState(position.x, position.y, position.z);
        if (state.mName == "minecraft:air")
            return false;

        const BlockData *data = BlockDataTable::find(state.mName.c_str());
        return data != nullptr && data->mSolid;
    }
}

bool BellOrientationBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:bell";
}

BlockState BellOrientationBlock::applyPlacementOrientation(const BlockState &state,
                                                           const BlockPlacementContext &context) const {
    using namespace PlacementOrientation;

    BlockState result = Block::applyPlacementOrientation(state, context);
    Tag states = result.mStates;
    const int facing = context.mFace >= FACE_NORTH ? context.mFace : context.mOppositeFacing;
    OrientationHelpers::setFacingDirection(states, facing);

    if (states.contains("attachment") && context.mFace != FACE_UP && context.mFace != FACE_DOWN
        && isSolidNeighbour(context.mLevel, relativePosition(context.mBlockPosition, context.mFace)))
        states.putString("attachment", "multiple");

    return BlockState(result.mName, states);
}
