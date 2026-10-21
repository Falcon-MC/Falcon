#include "Block/Blocks/ChorusPlantBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/Components/PlacementOrientation.h"
#include "Level/Level.h"

FALCON_REGISTER_BLOCK(ChorusPlantBlock, 333);

bool ChorusPlantBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:chorus_plant";
}

BlockState ChorusPlantBlock::connectedState(const BlockState &state,
                                            const std::function<const BlockState *(int)> &neighbour) {
    static const char *const properties[] = {
        "minecraft:connection_down", "minecraft:connection_up", "minecraft:connection_north",
        "minecraft:connection_south", "minecraft:connection_west", "minecraft:connection_east"
    };
    Tag states = state.mStates;
    for (int face = 0; face < 6; ++face) {
        const BlockState *other = neighbour(face);
        if (other == nullptr)
            continue;
        const bool connected = matches(other->mName) || other->mName == "minecraft:chorus_flower"
                               || (face == PlacementOrientation::FACE_DOWN && other->mName == "minecraft:end_stone");
        states.putByte(properties[face], connected ? 1 : 0);
    }
    return BlockState(state.mName, states);
}

BlockState ChorusPlantBlock::updateShape(Level &level, const Vector3i &position, const BlockState &state) const {
    return connectedState(state, [&level, &position](int face) {
        const Vector3i other = PlacementOrientation::relativePosition(position, face);
        return level.peekBlockPtr(other.x, other.y, other.z);
    });
}
