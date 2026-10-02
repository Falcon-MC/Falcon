#include "Block/Blocks/RedStoneWireBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(RedStoneWireBlock, 400);

#include "Block/Blocks/PlacementHelpers.h"
#include "Block/Components/PlacementOrientation.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"

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
