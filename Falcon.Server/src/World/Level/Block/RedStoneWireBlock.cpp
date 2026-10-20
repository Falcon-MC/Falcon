#include "World/Level/Block/RedStoneWireBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(RedStoneWireBlock, 400);

#include "World/Level/Block/PlacementHelpers.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"

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
