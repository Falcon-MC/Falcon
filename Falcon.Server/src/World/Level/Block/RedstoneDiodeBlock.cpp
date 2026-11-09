#include "World/Level/Block/RedstoneDiodeBlock.h"
#include "World/Level/Block/BlockSupportRules.h"

#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Level.h"

bool RedstoneDiodeBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const
{
    (void) blockFace;

    const BlockState below = level.getBlockState(position.x, position.y - 1, position.z);
    return BlockSupport::isAttachable(below, PlacementOrientation::FACE_UP) || below.mName == "minecraft:cauldron";
}

bool RedstoneDiodeBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const
{
    if (const std::optional<bool> measured = BlockSupportRules::survivesAt(level, position, state))
        return *measured;

    return canPlaceAt(level, position, PlacementOrientation::FACE_UP);
}

bool RedstoneDiodeBlock::isSignalSource() const
{
    return true;
}
