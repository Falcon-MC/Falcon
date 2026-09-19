#include "Block/Blocks/ChestBlock.h"

#include "Actor/ServerPlayer.h"
#include "Block/Actor/ChestBlockActor.h"
#include "Block/BlockActorStore.h"
#include "Inventory/Container/ChestContainerManagerModel.h"
#include "Level/Level.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"

namespace {
    const char *CHEST = "minecraft:chest";
    const char *TRAPPED_CHEST = "minecraft:trapped_chest";

    const int32_t NEIGHBOUR_OFFSETS[4][2] = {
            {1,  0},
            {-1, 0},
            {0,  1},
            {0,  -1}
    };

    std::string facingOf(const BlockState &state) {
        const Tag *states = state.mStates.get("minecraft:cardinal_direction");
        if (states != nullptr && states->getType() == Tag::Type::String)
            return states->asString();

        return std::string();
    }

    bool isPerpendicularToFacing(const std::string &facing, int32_t offsetX, int32_t offsetZ) {
        const bool facesEastWest = facing == "east" || facing == "west";
        const bool facesNorthSouth = facing == "north" || facing == "south";

        if (facesEastWest)
            return offsetX == 0;
        if (facesNorthSouth)
            return offsetZ == 0;

        return true;
    }
}

ChestBlock::ChestBlock(const Block &base) : Block(base) {
}

bool ChestBlock::matches(const std::string &identifier) {
    return identifier == CHEST || identifier == TRAPPED_CHEST;
}

ChestBlockActor &ChestBlock::getOrCreate(Level &level, const Vector3i &position) {
    (void) level;

    return BlockActorStore::getInstance().getOrCreate<ChestBlockActor>(position);
}

void ChestBlock::pair(Level &level, const Vector3i &position) {
    ChestBlockActor &placed = getOrCreate(level, position);
    if (placed.isPaired())
        return;

    const BlockState &state = level.getBlockState(position.x, position.y, position.z);
    const std::string facing = facingOf(state);

    for (const auto &offset: NEIGHBOUR_OFFSETS) {
        if (!isPerpendicularToFacing(facing, offset[0], offset[1]))
            continue;

        const Vector3i neighbour(position.x + offset[0], position.y, position.z + offset[1]);
        const BlockState &neighbourState = level.getBlockState(neighbour.x, neighbour.y, neighbour.z);
        if (neighbourState.mName != state.mName || facingOf(neighbourState) != facing)
            continue;

        ChestBlockActor *other = BlockActorStore::getInstance().find<ChestBlockActor>(neighbour);
        if (other == nullptr || other->isPaired())
            continue;

        placed.pairWith(*other);
        return;
    }
}

void ChestBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                          const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    (void) state;
    (void) usedItem;
    (void) blockFace;

    pair(owner.getLevelFor(player), position);
}

void ChestBlock::onBroken(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state) const {
    (void) state;

    ChestBlockActor *chest = BlockActorStore::getInstance().find<ChestBlockActor>(position);
    if (chest == nullptr)
        return;

    chest->unpair();

    const Vector3f dropPosition((float) position.x + 0.5f, (float) position.y + 0.5f,
                                (float) position.z + 0.5f);
    ChestInventory &inventory = chest->getInventory();
    for (int slot = 0; slot < ChestInventory::SIZE; ++slot) {
        const ItemStack &item = inventory.getContainerItem(slot);
        if (!item.isAir() && item.mCount > 0)
            ItemActorHandler::dropItem(owner, dropPosition, item, ItemActorHandler::randomDropMotion(),
                                       ItemActorHandler::DROP_PICKUP_DELAY);
    }

    BlockActorStore::getInstance().remove(position);
}

bool ChestBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                            const BlockState &state) const {
    (void) state;

    if (BlockActorStore::getInstance().find<ChestBlockActor>(position) == nullptr)
        pair(owner.getLevel(), position);

    ChestContainerManagerModel model;
    return model.open(owner, player, position);
}
