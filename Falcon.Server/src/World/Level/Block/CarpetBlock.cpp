#include "World/Level/Block/CarpetBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(CarpetBlock, 380);

#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/PlacementHelpers.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"

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
