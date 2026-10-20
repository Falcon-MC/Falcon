#include "World/Level/Block/TrapDoorBlock.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/OpenableBlock.h"
#include "World/Level/Block/Components/BlockPlacementComponent.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

FALCON_REGISTER_BLOCK(TrapDoorBlock, 130);

bool TrapDoorBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:trapdoor" || BlockIdentifier::endsWith(identifier, "_trapdoor");
}

BlockState TrapDoorBlock::applyPlacementOrientation(const BlockState &state,
                                                               const BlockPlacementContext &context) const {
    using namespace PlacementOrientation;

    BlockState result = Block::applyPlacementOrientation(state, context);
    Tag states = result.mStates;
    if (states.contains("direction"))
        states.putInt("direction", ewsnOrdinal(context.mOppositeFacing));
    return BlockState(result.mName, states);
}

bool TrapDoorBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player,
                                          const Vector3i &position, const BlockState &state) const {
    return OpenableBlock::toggle(owner, owner.getLevelFor(player), position, state);
}

void TrapDoorBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player,
                                        const Vector3i &position, const BlockState &state,
                                        const ItemStack &usedItem, int blockFace) const {
    (void) usedItem;
    (void) blockFace;

    OpenableBlock::openOnPlace(owner, owner.getLevelFor(player), position, state);
}
