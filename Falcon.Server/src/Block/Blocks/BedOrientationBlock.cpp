#include "Block/Blocks/BedOrientationBlock.h"

#include "Actor/ServerPlayer.h"
#include "Block/Actor/BedBlockActor.h"
#include "Block/BlockActorStore.h"
#include "Block/BlockClassRegistry.h"
#include "Block/Blocks/BedBlock.h"
#include "Block/Components/PlacementOrientation.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <memory>

FALCON_REGISTER_BLOCK(BedOrientationBlock, 220);

bool BedOrientationBlock::matches(const std::string &identifier) {
    return BedBlock::matches(identifier);
}

std::vector<BlockPlacementEntry> BedOrientationBlock::getPlacementBlocks(Level &level, const Vector3i &position,
                                                                        const BlockState &state,
                                                                        int playerFacing) const {
    (void) level;

    if (!state.mStates.contains("head_piece_bit") || playerFacing < PlacementOrientation::FACE_NORTH)
        return {};

    Tag states = state.mStates;
    states.putByte("head_piece_bit", 1);

    return {BlockPlacementEntry{PlacementOrientation::relativePosition(position, playerFacing),
                                BlockState(state.mName, states)}};
}

bool BedOrientationBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                     const BlockState &state) const {
    return BedBlock::use(owner, player, position, state);
}

void BedOrientationBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                   const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    (void) blockFace;

    Level &level = owner.getLevelFor(player);
    const int8_t color = (int8_t) std::clamp(usedItem.mDamage, 0, 15);

    const int facing = PlacementOrientation::horizontalFacing(player.getRotation().y);
    const Vector3i head = PlacementOrientation::relativePosition(position, facing);

    for (const Vector3i &half: {position, head}) {
        if (level.getBlockState(half.x, half.y, half.z).mName != state.mName)
            continue;

        level.getBlockActors().remove(half);

        std::unique_ptr<BlockActor> created(new BedBlockActor());
        created->setPosition(half);
        created->setState(state);
        static_cast<BedBlockActor *>(created.get())->setColor(color);

        const BlockActor &inserted = *created;
        level.getBlockActors().insert(std::move(created));
        BlockActionHandler::broadcastBlockActorData(owner, level, inserted);
    }
}

void BedOrientationBlock::onBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                   const BlockState &state) const {
    BedBlock::breakOtherHalf(owner, level, position, state);
}

std::vector<Vector3i> BedOrientationBlock::getAffectedBlocks(Level &level, const Vector3i &position,
                                                             const BlockState &state) const {
    return BedBlock::otherPiece(level, position, state);
}
