#include "Block/Blocks/TrapdoorOrientationBlock.h"

#include "Actor/ServerPlayer.h"
#include "Block/BlockClassRegistry.h"
#include "Block/BlockIdentifier.h"
#include "Block/Blocks/OpenableBlock.h"
#include "Block/Components/BlockPlacementComponent.h"
#include "Block/Components/PlacementOrientation.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

FALCON_REGISTER_BLOCK(TrapdoorOrientationBlock, 130);

bool TrapdoorOrientationBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:trapdoor" || BlockIdentifier::endsWith(identifier, "_trapdoor");
}

BlockState TrapdoorOrientationBlock::applyPlacementOrientation(const BlockState &state,
                                                               const BlockPlacementContext &context) const {
    using namespace PlacementOrientation;

    BlockState result = Block::applyPlacementOrientation(state, context);
    Tag states = result.mStates;
    if (states.contains("direction"))
        states.putInt("direction", ewsnOrdinal(context.mOppositeFacing));
    return BlockState(result.mName, states);
}

bool TrapdoorOrientationBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player,
                                          const Vector3i &position, const BlockState &state) const {
    return OpenableBlock::toggle(owner, owner.getLevelFor(player), position, state);
}

void TrapdoorOrientationBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player,
                                        const Vector3i &position, const BlockState &state,
                                        const ItemStack &usedItem, int blockFace) const {
    (void) usedItem;
    (void) blockFace;

    OpenableBlock::openOnPlace(owner, owner.getLevelFor(player), position, state);
}
