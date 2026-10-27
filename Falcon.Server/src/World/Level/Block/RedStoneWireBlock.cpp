#include "World/Level/Block/RedStoneWireBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(RedStoneWireBlock, 400);

#include "World/Level/Block/PlacementHelpers.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Block/TrapDoorBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"

using namespace PlacementHelpers;

bool RedStoneWireBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:redstone_wire";
}

bool RedStoneWireBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    (void) blockFace;

    return DecorationSupport::isSolid(belowOf(level, position));
}

bool RedStoneWireBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) state;
    return canPlaceAt(level, position, PlacementOrientation::FACE_UP);
}

BlockState RedStoneWireBlock::updateShape(Level &level, const Vector3i &position, const BlockState &state) const {
    const BlockState *above = level.peekBlockPtr(position.x, position.y + 1, position.z);
    if (above == nullptr)
        return state;
    const bool openAbove = !RedstoneSystem::isNormalBlock(*above);
    std::string sides[4];
    static const char *const properties[] = {"redstone_north", "redstone_south", "redstone_west", "redstone_east"};
    for (int index = 0; index < 4; ++index) {
        const int face = index + 2;
        const Vector3i other = PlacementOrientation::relativePosition(position, face);
        const BlockState *neighbour = level.peekBlockPtr(other.x, other.y, other.z);
        const BlockState *up = level.peekBlockPtr(other.x, other.y + 1, other.z);
        const BlockState *down = level.peekBlockPtr(other.x, other.y - 1, other.z);
        if (neighbour == nullptr || up == nullptr || down == nullptr)
            return state;
        const Block *block = VanillaBlocks::fromIdentifier(neighbour->mName);
        const bool solid = hasSolidFace(*neighbour, face ^ 1);
        const bool holdsWire = hasSolidFace(*neighbour, PlacementOrientation::FACE_UP)
                               || dynamic_cast<const TrapDoorBlock *>(block) != nullptr;
        if (openAbove && holdsWire
            && RedstoneSystem::canConnectWireTo(level, Vector3i(other.x, other.y + 1, other.z), RedstoneFace::NONE)) {
            sides[index] = solid ? "up" : "side";
        } else if (RedstoneSystem::canConnectWireTo(level, other, face)
                   || (!RedstoneSystem::isNormalBlock(*neighbour)
                       && RedstoneSystem::canConnectWireTo(level, Vector3i(other.x, other.y - 1, other.z), RedstoneFace::NONE))) {
            sides[index] = "side";
        } else {
            sides[index] = "none";
        }
    }
    const bool northSouthNone = sides[0] == "none" && sides[1] == "none";
    const bool eastWestNone = sides[2] == "none" && sides[3] == "none";
    Tag states = state.mStates;
    for (int index = 0; index < 4; ++index) {
        if (sides[index] == "none" && (index < 2 ? eastWestNone : northSouthNone))
            sides[index] = "side";
        states.putString(properties[index], sides[index]);
    }
    return BlockState(state.mName, states);
}
