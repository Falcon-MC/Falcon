#include "World/Level/Block/LecternBlock.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Containers/Managers/Models/BlockContainerManagerModel.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Level.h"

#include <cmath>

// before ContainerBlock, which matches the lectern too
FALCON_REGISTER_BLOCK(LecternBlock, 40);

namespace {
    const char *const SOUND_BOOK_PUT = "item.book.put";

    bool isBook(const ItemStack &item) {
        if (item.isAir() || item.mDefinition == nullptr)
            return false;
        const std::string &identifier = item.mDefinition->getIdentifier();
        return identifier == "minecraft:writable_book" || identifier == "minecraft:written_book";
    }
}

bool LecternBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:lectern";
}

bool LecternBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                              const BlockState &state) const {
    (void) state;
    Level &level = owner.getLevelFor(player);
    LecternBlockActor &lectern = level.getBlockActors().getOrCreate<LecternBlockActor>(position);
    if (lectern.hasBook()) {
        BlockContainerManagerModel model(ContainerType::Lectern);
        model.open(owner, player, position);
        return true;
    }

    PlayerInventory &inventory = player.getInventory();
    ItemStack held = inventory.getItemInHand();
    if (!isBook(held))
        return false;

    lectern.setBook(held);
    if (!player.isCreative()) {
        --held.mCount;
        if (held.mCount <= 0)
            held = ItemStack::air();
        inventory.setItemInHand(std::move(held));
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, inventory.getSelectedSlot());
    }
    BlockActionHandler::broadcastBlockActorData(owner, level, lectern);
    owner.playLevelSound(level, SOUND_BOOK_PUT, Vector3f((float) position.x, (float) position.y, (float) position.z));
    RedstoneSystem::queueRedstoneNotification(level, position);
    return true;
}

bool LecternBlock::onPunch(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                           const BlockState &state) const {
    (void) state;
    Level &level = owner.getLevelFor(player);
    LecternBlockActor *lectern = level.getBlockActors().find<LecternBlockActor>(position);
    if (lectern == nullptr || !lectern->hasBook())
        return false;

    const ItemStack book = lectern->takeBook();
    ItemActorHandler::dropItem(owner, level,
                               Vector3f((float) position.x + 0.5f, (float) position.y + 1.0f, (float) position.z + 0.5f),
                               book, ItemActorHandler::randomDropMotion(), ItemActorHandler::DROP_PICKUP_DELAY);
    BlockActionHandler::broadcastBlockActorData(owner, level, *lectern);
    RedstoneSystem::queueRedstoneNotification(level, position);
    return player.isCreative();
}

void LecternBlock::onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                     const BlockState &state) const {
    const bool powered = state.mStates.getByte("powered_bit", 0) != 0;
    Tag states = state.mStates;
    states.putByte("powered_bit", powered ? 0 : 1);
    const BlockState flipped(state.mName, states);
    level.setBlock(position, flipped, true);
    BlockActionHandler::broadcastBlockUpdate(owner, level, position, flipped);
    RedstoneSystem::queueRedstoneNotification(level, position);
    if (!powered)
        level.scheduleUpdate(position, 1);
}

int LecternBlock::getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) state;
    const LecternBlockActor *lectern = level.getBlockActors().find<LecternBlockActor>(position);
    if (lectern == nullptr || !lectern->hasBook())
        return 0;
    float signal = 14.0f;
    if (lectern->mTotalPages >= 4)
        signal = 14.0f * ((float) lectern->mPage / ((float) (lectern->mTotalPages / 2) - 1.0f));
    return (int) std::floor(signal) + 1;
}

void LecternBlock::pulse(Level &level, const Vector3i &position) {
    level.scheduleUpdate(position, 1);
}
