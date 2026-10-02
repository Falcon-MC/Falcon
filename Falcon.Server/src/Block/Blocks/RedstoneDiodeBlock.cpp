#include "Block/Blocks/RedstoneDiodeBlock.h"

#include "Block/BlockSupport.h"
#include "Block/Components/PlacementOrientation.h"
#include "Level/Level.h"

bool RedstoneDiodeBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const
{
    (void) blockFace;

    const BlockState below = level.getBlockState(position.x, position.y - 1, position.z);
    return BlockSupport::isAttachable(below, PlacementOrientation::FACE_UP) || below.mName == "minecraft:cauldron";
}

bool RedstoneDiodeBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const
{
    (void) state;

    return canPlaceAt(level, position, PlacementOrientation::FACE_UP);
}

bool RedstoneDiodeBlock::isSignalSource() const
{
    return true;
}
