#include "World/Level/Block/LadderBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockSupportRules.h"

FALCON_REGISTER_BLOCK(LadderBlock, 180);

#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/ThinFenceBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Level.h"

bool LadderBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:ladder";
}

bool LadderBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    if (blockFace == PlacementOrientation::FACE_DOWN || blockFace == PlacementOrientation::FACE_UP)
        return false;

    const Vector3i supportPosition = BlockSupport::supportOf(position, blockFace);
    const BlockState support = level.getBlockState(supportPosition.x, supportPosition.y, supportPosition.z);

    if (BlockIdentifier::endsWith(support.mName, "_stained_glass")
        || VanillaBlocks::getAs<ThinFenceBlock>(support.mName) != nullptr
        || BlockIdentifier::endsWith(support.mName, "_leaves") || support.mName == "minecraft:beacon")
        return false;

    return BlockSupport::isAttachable(support, blockFace);
}

// 3/16 plate against the wall: without it the server lets players walk through to the wall while the
// client stops and climbs
bool LadderBlock::getCollisionShape(const BlockState &state, const Actor *actor, AxisAlignedBB &shape) const {
    (void) actor;

    if (!state.mStates.contains("facing_direction"))
        return false;

    switch (state.mStates.getInt("facing_direction")) {
        case 2:
            shape = AxisAlignedBB(0.0f, 0.0f, 0.8125f, 1.0f, 1.0f, 1.0f);
            return true;
        case 3:
            shape = AxisAlignedBB(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.1875f);
            return true;
        case 4:
            shape = AxisAlignedBB(0.8125f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
            return true;
        case 5:
            shape = AxisAlignedBB(0.0f, 0.0f, 0.0f, 0.1875f, 1.0f, 1.0f);
            return true;
        default:
            return false;
    }
}

bool LadderBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    if (const std::optional<bool> measured = BlockSupportRules::survivesAt(level, position, state))
        return *measured;

    if (!state.mStates.contains("facing_direction"))
        return true;

    return canPlaceAt(level, position, state.mStates.getInt("facing_direction"));
}
