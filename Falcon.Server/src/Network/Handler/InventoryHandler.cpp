#include "Network/Handler/InventoryHandler.h"

#include "Core/Debug/BedrockLog.h"
#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Inventory/InventoryManager.h"
#include "Item/VanillaItems.h"
#include "Inventory/CraftingManager.h"
#include "Block/Inventory/CraftingTableInventory.h"
#include "Block/Inventory/FurnaceInventory.h"
#include "Inventory/BundleInventory.h"
#include "Inventory/ItemStackRequestHandler.h"
#include "Inventory/PlayerInventory.h"
#include "Item/EnchantmentHelper.h"
#include "Protocol/Packets/PlayerEnchantOptionsPacket.h"
#include "Protocol/Types/StartGameTypes.h"
#include "Network/Handler/BadPacketHandler.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/NetworkHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/ContainerClosePacket.h"
#include "Protocol/Packets/CraftingEventPacket.h"
#include "Protocol/Packets/InventoryTransactionPacket.h"
#include "Protocol/Packets/ItemStackRequestPacket.h"
#include "Protocol/Packets/ItemStackResponsePacket.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Packets/MobArmorEquipmentPacket.h"
#include "Protocol/Packets/MobEquipmentPacket.h"
#include "Protocol/Packets/PlayerHotbarPacket.h"
#include "Protocol/Types/ContainerSlotType.h"
#include "Protocol/Types/InventoryActionData.h"
#include "Protocol/Types/InventorySource.h"

#include <utility>

namespace {
    void playBundleSounds(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStackRequest &request,
                          bool succeeded) {
        bool inserted = false;
        bool removed = false;

        for (const ItemStackRequestAction &action: request.mActions) {
            if (action.mDestination.mContainerName.mContainer == ContainerSlotType::DynamicContainer)
                inserted = true;
            if (action.mSource.mContainerName.mContainer == ContainerSlotType::DynamicContainer)
                removed = true;
        }

        if (!inserted && !removed)
            return;

        Level &level = owner.getLevelFor(player);
        if (!succeeded) {
            if (inserted)
                owner.playLevelSound(level, LevelSoundEvent::BUNDLE_INSERT_FAIL, player.getPosition(),
                                     "minecraft:player");
            return;
        }

        if (inserted)
            owner.playLevelSound(level, LevelSoundEvent::BUNDLE_INSERT, player.getPosition(), "minecraft:player");
        else
            owner.playLevelSound(level, LevelSoundEvent::BUNDLE_REMOVE_ONE, player.getPosition(),
                                 "minecraft:player");
    }
}

void InventoryHandler::sendInventory(ServerNetworkHandler &owner, ServerPlayer &player) {
    std::vector<int> preparedSlots;
    BundleInventory::prepareBundleIds(player.getInventory(), preparedSlots);

    player.getInventoryManager().syncAll();
    if (player.getInventoryManager().isCraftingTableOpen()) {
        player.getInventoryManager().syncCraftingTableState(owner.getRecipeOutputs(), owner.getRecipeSourceIndices());
    } else {
        player.getInventoryManager().syncCraftingPreview(owner.getRecipeOutputs(), owner.getRecipeSourceIndices());
    }
    player.getInventoryManager().syncSelectedHotbarSlot();
    sendArmorContent(owner, player);
    sendOffhandContent(owner, player);
    sendHeldItem(owner, player);
}

void InventoryHandler::sendArmorContent(ServerNetworkHandler &owner, ServerPlayer &player) {
    const PlayerInventory &inventory = player.getInventory();

    MobArmorEquipmentPacket equipment;
    equipment.mRuntimeActorId = (int64_t) player.getRuntimeId();
    equipment.mHelmet = inventory.getArmor(PlayerInventory::ARMOR_HEAD);
    equipment.mChestplate = inventory.getArmor(PlayerInventory::ARMOR_TORSO);
    equipment.mLeggings = inventory.getArmor(PlayerInventory::ARMOR_LEGS);
    equipment.mBoots = inventory.getArmor(PlayerInventory::ARMOR_FEET);
    equipment.mBody = ItemStack::air();

    for (auto &entry: owner.getPlayers()) {
        if (entry.second.isSpawned() && &entry.second != &player)
            owner.getNetworkHandler().send(entry.second.getNetworkIdentifier(), equipment, owner.getCodecContext());
    }
}

void InventoryHandler::sendOffhandContent(ServerNetworkHandler &owner, ServerPlayer &player) {
    const ItemStack &offhand = player.getInventory().getOffhand();

    MobEquipmentPacket equipment;
    equipment.mRuntimeActorId = (int64_t) player.getRuntimeId();
    equipment.mItem = offhand;
    equipment.mInventorySlot = PlayerInventory::OFFHAND_NETWORK_SLOT;
    equipment.mHotbarSlot = PlayerInventory::OFFHAND_NETWORK_SLOT;
    equipment.mContainerId = PlayerInventory::CONTAINER_ID_OFFHAND;

    for (auto &entry: owner.getPlayers()) {
        if (entry.second.isSpawned())
            owner.getNetworkHandler().send(entry.second.getNetworkIdentifier(), equipment, owner.getCodecContext());
    }
}

void InventoryHandler::sendHeldItem(ServerNetworkHandler &owner, ServerPlayer &player) {
    const PlayerInventory &inventory = player.getInventory();
    const int selected = inventory.getSelectedSlot();

    player.getInventoryManager().syncSelectedHotbarSlot();

    MobEquipmentPacket equipment;
    equipment.mRuntimeActorId = (int64_t) player.getRuntimeId();
    equipment.mItem = inventory.getItemInHand();
    equipment.mInventorySlot = selected;
    equipment.mHotbarSlot = selected;
    equipment.mContainerId = PlayerInventory::CONTAINER_ID_INVENTORY;

    for (auto &entry: owner.getPlayers()) {
        if (entry.second.isSpawned() && &entry.second != &player)
            owner.getNetworkHandler().send(entry.second.getNetworkIdentifier(), equipment, owner.getCodecContext());
    }
}

void InventoryHandler::handleMobEquipment(ServerNetworkHandler &owner, ServerPlayer &player,
                                          const MobEquipmentPacket &packet) {
    if (packet.mContainerId != PlayerInventory::CONTAINER_ID_INVENTORY)
        return;

    PlayerInventory &inventory = player.getInventory();

    const int previousSlot = inventory.getSelectedSlot();

    player.getInventoryManager().onClientSelectHotbarSlot(packet.mHotbarSlot);
    inventory.setSelectedSlot(packet.mHotbarSlot);

    if (previousSlot != inventory.getSelectedSlot()) {
        const ItemStack &held = inventory.getItemInHand();
        const std::string identifier = held.mDefinition == nullptr ? "minecraft:air"
                                                                   : held.mDefinition->getIdentifier();
        PlayerHotbarSelectedSlotChangeAfterEvent event(player, inventory.getSelectedSlot(), previousSlot,
                                                       identifier);
        owner.getEventBus().after().mPlayerHotbarSelectedSlotChange.emit(event);
    }

    MobEquipmentPacket equipment;
    equipment.mRuntimeActorId = (int64_t) player.getRuntimeId();
    equipment.mItem = inventory.getItemInHand();
    equipment.mInventorySlot = inventory.getSelectedSlot();
    equipment.mHotbarSlot = inventory.getSelectedSlot();
    equipment.mContainerId = PlayerInventory::CONTAINER_ID_INVENTORY;

    for (auto &entry: owner.getPlayers()) {
        if (entry.second.isSpawned() && &entry.second != &player)
            owner.getNetworkHandler().send(entry.second.getNetworkIdentifier(), equipment, owner.getCodecContext());
    }
}

void InventoryHandler::handlePlayerHotbar(ServerNetworkHandler &owner, ServerPlayer &player,
                                          const PlayerHotbarPacket &packet) {
    if (packet.mContainerId != PlayerInventory::CONTAINER_ID_INVENTORY || !packet.mSelectHotbarSlot)
        return;

    if (packet.mSelectedHotbarSlot < 0 || packet.mSelectedHotbarSlot >= PlayerInventory::HOTBAR_SIZE) {
        sendHeldItem(owner, player);
        return;
    }

    player.getInventoryManager().onClientSelectHotbarSlot(packet.mSelectedHotbarSlot);
    player.getInventory().setSelectedSlot(packet.mSelectedHotbarSlot);
}

void InventoryHandler::handleOpenInventory(ServerNetworkHandler &owner, ServerPlayer &player) {
    player.getInventoryManager().onClientOpenMainInventory();
    player.getInventoryManager().syncContents(InventoryManager::InventoryId::CraftingInput);
    player.getInventoryManager().syncCraftingPreview(owner.getRecipeOutputs(), owner.getRecipeSourceIndices());
}

void InventoryHandler::handleItemStackRequest(ServerNetworkHandler &owner, const NetworkIdentifier &id,
                                              ServerPlayer &player, const ItemStackRequestPacket &packet) {
    PlayerInventory &inventory = player.getInventory();
    const ItemStack offhandBefore = inventory.getOffhand();

    ItemStackResponsePacket response;
    bool needsResync = false;

    std::vector<ItemStack> droppedItems;
    std::vector<BundleSyncData> bundles;

    std::vector<int> preparedSlots;
    bool preparedBundles = BundleInventory::prepareBundleIds(inventory, preparedSlots);

    Container *openContainer = player.getInventoryManager().getContainer();
    if (openContainer != nullptr && BundleInventory::prepareBundleIds(*openContainer))
        preparedBundles = true;

    if (preparedBundles) {
        for (int slot: preparedSlots)
            player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
        player.getInventoryManager().syncBundles();
    }

    for (const ItemStackRequest &request: packet.mRequests) {
        const int32_t gameType = player.getGameType();
        const bool creativeMode = gameType == (int32_t) GameType::Creative
                                  || gameType == (int32_t) GameType::Spectator;
        int32_t enchantLevelsConsumed = 0;

        ItemStackResponseEntry entry = ItemStackRequestHandler::execute(inventory, request, owner.getCreativeItems(),
                                                                       owner.getRecipeOutputs(),
                                                                       owner.getRecipeSourceIndices(),
                                                                       player.getInventoryManager().isCraftingTableOpen(),
                                                                       player.getInventoryManager().isFurnaceOpen(),
                                                                       player.getInventoryManager().getContainer(),
                                                                       &droppedItems,
                                                                       &owner.getCodecContext(),
                                                                       &bundles,
                                                                       player.getExperience().getXpLevel(),
                                                                       creativeMode,
                                                                       &enchantLevelsConsumed);

        if (entry.mResult == ItemStackRequestHandler::RESULT_OK && enchantLevelsConsumed > 0) {
            player.getExperience().subtractXpLevels(enchantLevelsConsumed);
            player.syncExperience();
            owner._sendAttributes(player);
        }

        if (entry.mResult != ItemStackRequestHandler::RESULT_OK) {
            needsResync = true;
            playBundleSounds(owner, player, request, false);
        } else {
            playBundleSounds(owner, player, request, true);
        }

        response.mEntries.push_back(std::move(entry));
    }

    for (const BundleSyncData &bundle: bundles) {
        const ItemStack *owned = inventory.resolveSlot(bundle.mOwnerContainer, bundle.mOwnerSlot);
        if (owned == nullptr)
            continue;

        player.getInventoryManager().syncBundle(*owned, bundle.mBundleId, bundle.mContents);

        if (bundle.mOwnerContainer == ContainerSlotType::Inventory)
            player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, bundle.mOwnerSlot);
        else if (bundle.mOwnerContainer == ContainerSlotType::Cursor)
            player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Cursor, 0);
        else if (bundle.mOwnerContainer == ContainerSlotType::Offhand)
            player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Offhand, 0);
    }

    for (const ItemStack &dropped: droppedItems)
        owner._throwItem(player, dropped);

    player.getInventoryManager().syncCraftingPreview(owner.getRecipeOutputs(), owner.getRecipeSourceIndices());
    if (player.getInventoryManager().isFurnaceOpen()) {
        player.getInventoryManager().syncContents(InventoryManager::InventoryId::FurnaceInput);
    }

    _updateEnchantOptions(owner, player);

    const ItemStack &offhandAfter = inventory.getOffhand();
    if (offhandAfter.mDefinition != offhandBefore.mDefinition || offhandAfter.mCount != offhandBefore.mCount
        || offhandAfter.mDamage != offhandBefore.mDamage)
        sendOffhandContent(owner, player);

    if (player.getInventoryManager().isContainerOpen())
        owner.refreshContainerViewers(player.getInventoryManager().getContainerPosition(), &player);

    if (response.mEntries.empty())
        return;

    owner.getNetworkHandler().send(id, response, owner.getCodecContext());

    if (needsResync)
        owner._sendInventory(player);
}

void InventoryHandler::_updateEnchantOptions(ServerNetworkHandler &owner, ServerPlayer &player) {
    InventoryManager &manager = player.getInventoryManager();
    if (manager.getCurrentWindowType() != ContainerType::Enchantment || !manager.isContainerOpen())
        return;

    const ItemStack *input = player.getInventory().resolveSlot(ContainerSlotType::EnchantingInput, 0);
    const ItemStack empty;
    const ItemStack &current = input == nullptr ? empty : *input;

    const int32_t seed = (int32_t) (owner.getCurrentTick() * 31 + manager.getContainerPosition().x * 7
                                    + manager.getContainerPosition().z);
    if (!manager.refreshEnchantInput(current, seed))
        return;

    PlayerEnchantOptionsPacket packet;
    if (!current.isAir())
        packet.mOptions = EnchantmentHelper::getEnchantOptions(owner.getLevelFor(player), manager.getContainerPosition(),
                                                               current, manager.getEnchantSeed());

    owner.getNetworkHandler().send(player.getNetworkIdentifier(), packet, owner.getCodecContext());
}

void InventoryHandler::handleTransaction(ServerNetworkHandler &owner, ServerPlayer &player,
                                         const InventoryTransactionPacket &packet) {
    if (packet.mTransactionType == InventoryTransactionType::ItemUseOnEntity) {
        if (packet.mHotbarSlot != player.getInventory().getSelectedSlot()) {
            owner._sendInventory(player);
            return;
        }

        if (packet.mActionType == 1 || packet.mActionType == 2) {
            player.attackActor(owner, (uint64_t) packet.mRuntimeActorId);
            return;
        }

        if (packet.mActionType == 0) {
            ServerActor *target = owner.getActor((int64_t) packet.mRuntimeActorId);
            if (target != nullptr && !owner.getScriptEngine().beforePlayerInteractWithEntity(player, *target)) {
                target = owner.getActor((int64_t) packet.mRuntimeActorId);
                if (target != nullptr)
                    target->onInteract(owner, player);
            }
        }
        return;
    }

    if (packet.mTransactionType == InventoryTransactionType::ItemUse) {
        if (packet.mActionType == 0) {
            ItemUseTransaction transaction;
            transaction.mLegacyRequestId = packet.mLegacyRequestId;
            transaction.mActionType = packet.mActionType;
            transaction.mBlockPosition = packet.mBlockPosition;
            transaction.mBlockFace = packet.mBlockFace;
            transaction.mHotbarSlot = packet.mHotbarSlot;
            transaction.mItemInHand = packet.mItemInHand;
            transaction.mPlayerPosition = packet.mPlayerPosition;
            transaction.mClickPosition = packet.mClickPosition;
            transaction.mBlockDefinition = packet.mBlockDefinition;
            transaction.mClientInteractPrediction = packet.mClientInteractPrediction;
            transaction.mTriggerType = packet.mTriggerType;
            transaction.mClientCooldownState = packet.mClientCooldownState;
            BlockActionHandler::placeBlock(owner, player, transaction);
        } else if (packet.mActionType == 1) {
            owner._useHeldItem(player);
        } else if (packet.mActionType == 2) {
            BlockActionHandler::completeBreakingBlock(owner, player, packet.mBlockPosition);
        }
        return;
    }

    if (packet.mTransactionType == InventoryTransactionType::ItemRelease) {
        const bool wasUsing = player.getFlags().get(ActorFlag::UsingItem);

        const ItemStack &releasedItem = player.getInventory().getItemInHand();
        if (wasUsing && !releasedItem.isAir() && releasedItem.mDefinition != nullptr) {
            const Item *itemType = VanillaItems::fromIdentifier(releasedItem.mDefinition->getIdentifier());
            const int32_t elapsedTicks = (int32_t) (owner.getCurrentTick() - player.getItemUseStartTick());
            if (itemType != nullptr && itemType->onStopUsing(owner, player, releasedItem, elapsedTicks)) {
                player.getFlags().set(ActorFlag::UsingItem, false);
                owner._sendEntityData(player);
                return;
            }
        }

        owner._consumeHeldItem(player);

        if (packet.mActionType == 0) {
            player.clearAwaitingConsumableRelease();

            if (wasUsing && player.getFlags().get(ActorFlag::UsingItem)) {
                player.setLastEarlyConsumableReleaseTick(owner.getCurrentTick());
                player.getFlags().set(ActorFlag::UsingItem, false);
                owner._sendEntityData(player);
            }
        }
        return;
    }

    if (packet.mTransactionType != InventoryTransactionType::Normal
        && packet.mTransactionType != InventoryTransactionType::Mismatch)
        return;

    PlayerInventory &inventory = player.getInventory();

    int32_t sourceSlot = -1;
    int32_t droppedCount = -1;

    for (const InventoryActionData &action: packet.mActions) {
        if (action.mSource.mType == InventorySourceType::WorldInteraction && action.mSlot == 0) {
            droppedCount = action.mToItem.mCount;
            continue;
        }

        if (action.mSource.mType == InventorySourceType::Container
            && action.mSource.mContainerId == PlayerInventory::CONTAINER_ID_INVENTORY) {
            sourceSlot = action.mSlot;
        }
    }

    if (sourceSlot < 0 || droppedCount <= 0)
        return;

    ItemStack *item = inventory.resolveSlot(ContainerSlotType::Inventory, sourceSlot);
    if (item == nullptr || item->isAir() || droppedCount > item->mCount)
        return;

    ItemStack thrown = *item;
    thrown.mCount = droppedCount;
    thrown.mUsingNetId = false;
    thrown.mNetId = 0;

    item->mCount -= droppedCount;
    if (item->mCount <= 0)
        *item = ItemStack::air();

    inventory.assignNetId(*item);
    owner._throwItem(player, thrown);

    player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, sourceSlot);
}

void InventoryHandler::handleContainerClose(ServerPlayer &player, const ContainerClosePacket &packet) {
    PlayerInventory &inventory = player.getInventory();
    InventoryManager &manager = player.getInventoryManager();
    int windowId = (int) packet.mWindowId;
    if (windowId == InventoryManager::CONTAINER_ID_NONE) {
        windowId = manager.getCurrentWindowId();
    }
    const bool craftingTable = manager.isCraftingTableOpen()
                               && windowId == manager.getCraftingTableWindowId();
    const bool furnace = manager.isFurnaceOpen() && windowId == manager.getFurnaceWindowId();
    const int gridSize = craftingTable ? CraftingTableInventory::SIZE
                       : furnace ? FurnaceInventory::SIZE : PlayerInventory::CRAFTING_SIZE;

    for (int slot = 0; slot < gridSize; slot++) {
        if (furnace) {
            continue;
        }
        ItemStack item = craftingTable ? inventory.getCraftingTableItem(slot)
                         : furnace ? inventory.getFurnaceItem(slot) : inventory.getCraftingItem(slot);
        if (item.isAir()) {
            continue;
        }

        std::vector<int> touchedSlots;
        const int remaining = inventory.addItemPartial(item, touchedSlots);
        item.mCount = remaining;
        if (craftingTable) {
            inventory.setCraftingTableItem(slot, std::move(item));
        } else if (furnace) {
            inventory.setFurnaceItem(slot, std::move(item));
        } else {
            inventory.setCraftingItem(slot, std::move(item));
        }

        for (int touchedSlot: touchedSlots)
            player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, touchedSlot);

        manager.syncSlot(craftingTable ? InventoryManager::InventoryId::CraftingTableInput
                       : furnace ? InventoryManager::InventoryId::FurnaceInput
                                 : InventoryManager::InventoryId::CraftingInput, slot);
    }

    if (!inventory.getCursor().isAir()) {
        std::vector<int> touchedSlots;
        ItemStack cursor = inventory.getCursor();
        const int remaining = inventory.addItemPartial(cursor, touchedSlots);
        cursor.mCount = remaining;
        inventory.setCursor(std::move(cursor));

        for (int slot: touchedSlots)
            player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);

        manager.syncSlot(InventoryManager::InventoryId::Cursor, 0);
    }

    manager.onClientRemoveWindow(windowId);
}

void InventoryHandler::handleCraftingEvent(ServerNetworkHandler &owner, ServerPlayer &player,
                                           const CraftingEventPacket &packet) {
    const bool table = packet.mType == CraftingType::Workbench;
    if (table && !player.getInventoryManager().isCraftingTableOpen()) {
        return;
    }

    const int gridWidth = table ? 3 : 2;
    const PlayerInventory &inventory = player.getInventory();
    const std::vector<ItemStack> &grid = table ? inventory.getCraftingTableContents()
                                               : inventory.getCraftingContents();
    if (packet.mOutputs.empty()) {
        return;
    }

    CraftingRecipeMatch match;
    const std::vector<ItemStack> &recipeOutputs = owner.getRecipeOutputs();
    const std::vector<uint32_t> &recipeSourceIndices = owner.getRecipeSourceIndices();
    if (!CraftingManager::matchResult(grid, gridWidth, packet.mOutputs.front(), recipeOutputs,
                                      recipeSourceIndices, match)) {
        return;
    }

    PlayerInventory working = inventory;
    const std::vector<ItemStack> &workingGrid = table ? working.getCraftingTableContents()
                                                      : working.getCraftingContents();
    if (match.mRequiredBySlot.size() != workingGrid.size()) {
        return;
    }

    for (size_t slot = 0; slot < match.mRequiredBySlot.size(); ++slot) {
        const int required = match.mRequiredBySlot[slot];
        if (required <= 0) {
            continue;
        }
        if (workingGrid[slot].mCount < required) {
            return;
        }

        ItemStack remaining = workingGrid[slot];
        remaining.mCount -= required;
        if (remaining.mCount <= 0) {
            remaining = ItemStack::air();
        }
        if (table) {
            working.setCraftingTableItem((int) slot, std::move(remaining));
        } else {
            working.setCraftingItem((int) slot, std::move(remaining));
        }
    }

    ItemStack output = packet.mOutputs.front();
    if (!working.addItem(output)) {
        return;
    }

    player.getInventory() = std::move(working);
    player.getInventoryManager().syncAll();
    if (table) {
        player.getInventoryManager().syncCraftingTableState(recipeOutputs, recipeSourceIndices);
    } else {
        player.getInventoryManager().syncCraftingPreview(recipeOutputs, recipeSourceIndices);
    }
}
