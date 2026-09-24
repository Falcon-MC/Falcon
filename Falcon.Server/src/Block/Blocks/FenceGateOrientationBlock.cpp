#include "Block/Blocks/FenceGateOrientationBlock.h"

#include "Actor/ServerPlayer.h"
#include "Block/BlockClassRegistry.h"
#include "Block/BlockIdentifier.h"
#include "Block/Blocks/OpenableBlock.h"
#include "Block/Components/PlacementOrientation.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>

FALCON_REGISTER_BLOCK(FenceGateOrientationBlock, 225);

bool FenceGateOrientationBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:fence_gate" || BlockIdentifier::endsWith(identifier, "_fence_gate");
}

bool FenceGateOrientationBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player,
                                           const Vector3i &position, const BlockState &state) const {
    using namespace PlacementOrientation;

    Level &level = owner.getLevelFor(player);

    const std::string facing = state.mStates.getString("minecraft:cardinal_direction", "south");
    const bool alongZ = facing == "north" || facing == "south";

    float rotation = std::fmod(player.getRotation().y - 90.0f, 360.0f);
    if (rotation < 0.0f)
        rotation += 360.0f;

    const int swing = alongZ
                      ? (rotation < 180.0f ? FACE_NORTH : FACE_SOUTH)
                      : (rotation >= 90.0f && rotation < 270.0f ? FACE_EAST : FACE_WEST);

    Tag states = state.mStates;
    states.putString("minecraft:cardinal_direction", cardinalName(swing));

    return OpenableBlock::toggle(owner, level, position, BlockState(state.mName, states));
}

void FenceGateOrientationBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player,
                                         const Vector3i &position, const BlockState &state,
                                         const ItemStack &usedItem, int blockFace) const {
    (void) usedItem;
    (void) blockFace;

    OpenableBlock::openOnPlace(owner, owner.getLevelFor(player), position, state);
}
