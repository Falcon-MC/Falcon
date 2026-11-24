#include "World/Level/Block/GrindstoneBlock.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Level.h"

FALCON_REGISTER_BLOCK(GrindstoneBlock, 10);

bool GrindstoneBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:grindstone";
}

bool GrindstoneBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    const Vector3i support = BlockSupport::supportOf(position, blockFace);
    return BlockSupport::isAttachable(level.getBlockState(support.x, support.y, support.z), blockFace);
}

bool GrindstoneBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    using namespace PlacementOrientation;
    const std::string attachment = state.mStates.getString("attachment", "standing");
    if (attachment == "standing")
        return canPlaceAt(level, position, FACE_UP);
    if (attachment == "hanging")
        return canPlaceAt(level, position, FACE_DOWN);

    static constexpr int faces[] = {FACE_SOUTH, FACE_WEST, FACE_NORTH, FACE_EAST};
    const int direction = state.mStates.getInt("direction", 0);
    return direction >= 0 && direction < 4 && canPlaceAt(level, position, faces[direction]);
}

bool GrindstoneBlock::onInteract(ServerNetworkHandler &, ServerPlayer &player, const Vector3i &position,
                                 const BlockState &) const {
    return player.getInventoryManager().onClientOpenGrindstone(position);
}
