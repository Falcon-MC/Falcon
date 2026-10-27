#include "World/Level/Block/FireBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/PlacementHelpers.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Level.h"

FALCON_REGISTER_BLOCK(FireBlock, 326);

bool FireBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:fire" || identifier == "minecraft:soul_fire";
}

bool FireBlock::canBeReplaced(const BlockState &state) const {
    (void) state;

    return true;
}

BlockState FireBlock::updateShape(Level &level, const Vector3i &position, const BlockState &state) const {
    if (!state.mStates.contains("minecraft:connection_up"))
        return state;

    const BlockData *neighbours[6] = {};
    bool supported = false;
    for (int face = 0; face < 6; ++face) {
        const Vector3i other = PlacementOrientation::relativePosition(position, face);
        const BlockState *block = level.peekBlockPtr(other.x, other.y, other.z);
        if (block == nullptr)
            return state;
        neighbours[face] = BlockDataTable::find(block->mName.c_str());
        if (face == 0)
            supported = PlacementHelpers::hasSolidFace(*block, PlacementOrientation::FACE_UP);
    }

    static const char *const properties[] = {
        "", "minecraft:connection_up", "minecraft:connection_north", "minecraft:connection_south",
        "minecraft:connection_west", "minecraft:connection_east"
    };
    const BlockData *down = neighbours[0];
    const bool grounded = supported || (down != nullptr && down->mBurnChance > 0);
    Tag states = state.mStates;
    for (int face = 1; face < 6; ++face)
        states.putByte(properties[face], !grounded && neighbours[face] != nullptr
                                       && neighbours[face]->mBurnChance > 0 ? 1 : 0);
    return BlockState(state.mName, states);
}
