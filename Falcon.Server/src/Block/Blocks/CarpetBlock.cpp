#include "Block/Blocks/CarpetBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(CarpetBlock, 380);

#include "Block/BlockIdentifier.h"
#include "Block/Blocks/PlacementHelpers.h"
#include "Block/Components/PlacementOrientation.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"

using namespace PlacementHelpers;

bool CarpetBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_carpet");
}

bool CarpetBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    (void) blockFace;

    return !DecorationSupport::isAir(belowOf(level, position));
}

bool CarpetBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) state;
    return canPlaceAt(level, position, PlacementOrientation::FACE_UP);
}
