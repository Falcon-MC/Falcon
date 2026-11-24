#include "Network/Handler/InventoryHandler.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "Network/Crypto/Base64.h"
#include "World/Item/EnchantmentData.h"
#include "World/Actor/ServerActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Item/ItemProperties.h"
#include "World/Item/VanillaItems.h"
#include "World/Inventory/CraftingManager.h"
#include "World/Level/Block/Inventory/CraftingTableInventory.h"
#include "World/Level/Block/Inventory/FurnaceInventory.h"
#include "World/Inventory/BundleInventory.h"
#include "World/Inventory/ItemStackRequestHandler.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Item/MapItem.h"
#include "World/Item/PackShooter.h"
#include "World/Level/Block/Actor/BeaconBlockActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/AnvilBlock.h"
#include "World/Level/Block/CartographyTableBlock.h"
#include "World/Level/Block/StonecutterBlock.h"
#include "World/Level/Block/SmithingTableBlock.h"
#include "World/Level/Block/GrindstoneBlock.h"
#include "World/Level/Block/LoomBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Item/EnchantmentHelper.h"
#include "Protocol/Packets/PlayerEnchantOptionsPacket.h"
#include "Protocol/Types/StartGameTypes.h"
#include "Network/Handler/BadPacketHandler.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/NetworkHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Scripting/Content/CustomContentRegistry.h"
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
#include "Falcon/Plugin/PluginEvents.h"

#include <algorithm>
#include <random>
#include <string>
#include <utility>

namespace {
    std::string encodeEnchantOptions(const std::vector<EnchantOptionData> &options) {
        std::string json = "[";
        for (const auto &option: options) {
            if (json.size() > 1)
                json += ',';
            json += '[' + std::to_string(option.mCost) + ",\"" + Base64::encode(option.mEnchantName) + "\",[";
            bool first = true;
            for (const auto &enchant: option.mEnchants0) {
                if (!first)
                    json += ',';
                first = false;
                json += '[' + std::to_string(enchant.mType) + ',' + std::to_string(enchant.mLevel) + ']';
            }
            json += "]]";
        }
        return json + ']';
    }

    bool decodeEnchantOptions(const std::string &data, std::vector<EnchantOptionData> &result) {
        const auto list = json::parse(data);
        if (list == nullptr || !list->isArray() || list->mArray.size() > 3)
            return false;

        for (const auto &entry: list->mArray) {
            if (entry == nullptr || !entry->isArray() || entry->mArray.size() != 3)
                return false;
            const auto &fields = entry->mArray;
            const int32_t cost = fields[0]->integer(-1);
            if (cost < 0 || cost > 32767 || !fields[1]->isString() || !fields[2]->isArray()
                || fields[2]->mArray.size() > 64)
                return false;

            EnchantOptionData option;
            option.mCost = cost;
            const std::string &encodedName = fields[1]->mString;
            if (encodedName.size() > 44'000
                || (!encodedName.empty() && !Base64::decode(encodedName, option.mEnchantName))
                || option.mEnchantName.size() > 32767)
                return false;
            for (const auto &enchant: fields[2]->mArray) {
                if (enchant == nullptr || !enchant->isArray() || enchant->mArray.size() != 2)
                    return false;
                const int32_t id = enchant->mArray[0]->integer(-1);
                const int32_t level = enchant->mArray[1]->integer(-1);
                if (EnchantmentTable::findById(id) == nullptr || level <= 0 || level > 32767)
                    return false;
                option.mEnchants0.push_back({id, level});
            }
            result.push_back(std::move(option));
        }
        return true;
    }

    void returnToInventory(ServerPlayer &player, ItemStack &item) {
        std::vector<int> touchedSlots;
        item.mCount = player.getInventory().addItemPartial(item, touchedSlots);
        if (item.mCount <= 0)
            item = ItemStack::air();

        for (int slot: touchedSlots)
            player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
    }

    bool isBlockContainer(ContainerSlotType type) {
        return type == ContainerSlotType::LevelEntity || type == ContainerSlotType::Barrel
               || type == ContainerSlotType::ShulkerBox || type == ContainerSlotType::CrafterBlockContainer;
    }

    std::string containerName(ContainerSlotType type) {
        if (isBlockContainer(type))
            return "container";

        switch (type) {
            case ContainerSlotType::Hotbar:
                return "hotbar";
            case ContainerSlotType::Inventory:
                return "inventory";
            case ContainerSlotType::HotbarAndInventory:
                return "hotbar_and_inventory";
            case ContainerSlotType::Armor:
                return "armor";
            case ContainerSlotType::Offhand:
                return "offhand";
            case ContainerSlotType::Cursor:
                return "cursor";
            case ContainerSlotType::CraftingInput:
                return "crafting_input";
            case ContainerSlotType::CraftingOutput:
            case ContainerSlotType::CreatedOutput:
                return "crafting_output";
            case ContainerSlotType::FurnaceIngredient:
            case ContainerSlotType::BlastFurnaceIngredient:
            case ContainerSlotType::SmokerIngredient:
                return "furnace_ingredient";
            case ContainerSlotType::FurnaceFuel:
                return "furnace_fuel";
            case ContainerSlotType::FurnaceResult:
                return "furnace_result";
            case ContainerSlotType::EnchantingInput:
                return "enchanting_input";
            case ContainerSlotType::EnchantingMaterial:
                return "enchanting_material";
            case ContainerSlotType::DynamicContainer:
                return "bundle";
            default:
                return "other";
        }
    }

    const char *transactionName(ItemStackRequestActionType type) {
        switch (type) {
            case ItemStackRequestActionType::Take:
                return "take";
            case ItemStackRequestActionType::Place:
                return "place";
            case ItemStackRequestActionType::Swap:
                return "swap";
            case ItemStackRequestActionType::Drop:
                return "drop";
            case ItemStackRequestActionType::Destroy:
                return "destroy";
            case ItemStackRequestActionType::Consume:
                return "consume";
            default:
                return nullptr;
        }
    }

    bool hasDestination(ItemStackRequestActionType type) {
        return type == ItemStackRequestActionType::Take || type == ItemStackRequestActionType::Place
               || type == ItemStackRequestActionType::Swap;
    }

    ItemStack itemAt(const PlayerInventory &inventory, Container *openContainer, ContainerSlotType type, int slot) {
        if (isBlockContainer(type)) {
            if (openContainer == nullptr || slot < 0 || slot >= openContainer->getContainerSize())
                return ItemStack::air();
            return openContainer->getContainerItem(slot);
        }

        const ItemStack *item = inventory.resolveSlot(type, slot);
        return item == nullptr ? ItemStack::air() : *item;
    }

    // ServerPlayer::attackActor bounds a hit the same way
    const float ENTITY_INTERACT_REACH = 8.0f;

    // LecternHandler's widest reach, as a safety bound: reach is checked when a window opens, and a player
    // who then leaves must not keep the container
    const float OPEN_CONTAINER_REACH = 14.0f;

    bool withinContainerReach(const ServerPlayer &player, const Vector3f &center) {
        const Vector3f &from = player.getPosition();
        const float dx = from.x - center.x, dy = from.y - center.y, dz = from.z - center.z;
        return dx * dx + dy * dy + dz * dz <= OPEN_CONTAINER_REACH * OPEN_CONTAINER_REACH;
    }

    bool hasLeftOpenContainer(ServerNetworkHandler &owner, ServerPlayer &player) {
        InventoryManager &manager = player.getInventoryManager();
        if (!manager.isContainerOpen())
            return false;

        if (manager.getContainerActorId() != -1) {
            const ServerActor *actor = owner.getActor(manager.getContainerActorId());
            return actor != nullptr
                   && (!actor->sharesLevelWith(player) || !withinContainerReach(player, actor->getPosition()));
        }

        const Vector3i &position = manager.getContainerPosition();
        return !withinContainerReach(
                player, Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f));
    }

    bool isEnchantingSlot(ContainerSlotType type) {
        return type == ContainerSlotType::EnchantingInput || type == ContainerSlotType::EnchantingMaterial;
    }

    bool isEnchantCraft(const ItemStackRequestAction &action) {
        return (action.mType == ItemStackRequestActionType::CraftRecipe
                || action.mType == ItemStackRequestActionType::CraftRecipeAuto)
               && action.mRecipeNetworkId >= EnchantmentHelper::RECIPE_ID_BASE;
    }

    bool usesEnchantingTable(const ItemStackRequestAction &action) {
        if (isEnchantingSlot(action.mSource.mContainerName.mContainer)
            || isEnchantingSlot(action.mDestination.mContainerName.mContainer))
            return true;

        return isEnchantCraft(action);
    }

    bool craftsEnchant(const ItemStackRequest &request) {
        for (const ItemStackRequestAction &action: request.mActions) {
            if (isEnchantCraft(action))
                return true;
        }
        return false;
    }

    // an option belongs to the player it was sent to and to the item then in the table: not another player's
    // net id, nor an item swapped in by an earlier action of the same request
    bool takesForeignEnchant(const InventoryManager &manager, const PlayerInventory &inventory,
                             const ItemStackRequest &request) {
        const ItemStack *input = inventory.resolveSlot(ContainerSlotType::EnchantingInput, 0);
        bool inputMoved = false;
        for (const ItemStackRequestAction &action: request.mActions) {
            if (isEnchantCraft(action)
                && (inputMoved || input == nullptr || !manager.offersEnchant(action.mRecipeNetworkId, *input)))
                return true;

            if (action.mSource.mContainerName.mContainer == ContainerSlotType::EnchantingInput
                || action.mDestination.mContainerName.mContainer == ContainerSlotType::EnchantingInput)
                inputMoved = true;
        }
        return false;
    }

    // the enchanting table's two slots and the beacon's payment live in the player inventory: nothing else gives
    // them back
    bool returnScreenItems(ServerPlayer &player) {
        bool returned = false;
        const std::pair<ContainerSlotType, int> slots[] = {
                {ContainerSlotType::EnchantingInput, 0},
                {ContainerSlotType::EnchantingMaterial, 0},
                {ContainerSlotType::BeaconPayment, PlayerInventory::BEACON_PAYMENT_NETWORK_SLOT}};
        for (const auto &[type, index]: slots) {
            ItemStack *slot = player.getInventory().resolveSlot(type, index);
            if (slot == nullptr || slot->isAir())
                continue;

            ItemStack item = *slot;
            returnToInventory(player, item);
            *slot = std::move(item);
            returned = true;
        }
        return returned;
    }

    bool allowRequest(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStackRequest &request) {
        PluginManager &plugins = owner.getPluginManager();
        const bool transactions = plugins.hasSubscribers(FALCON_EVENT_INVENTORY_TRANSACTION);
        if (!transactions)
            return true;

        const PlayerInventory &inventory = player.getInventory();
        Container *openContainer = player.getInventoryManager().getContainer();

        for (const ItemStackRequestAction &action: request.mActions) {
            const char *name = transactionName(action.mType);
            if (!transactions || name == nullptr)
                continue;

            const ContainerSlotType sourceType = action.mSource.mContainerName.mContainer;
            ItemStack item = itemAt(inventory, openContainer, sourceType, action.mSource.mSlot);
            PluginEvent transactionEvent;
            transactionEvent.mType = FALCON_EVENT_INVENTORY_TRANSACTION;
            transactionEvent.mCancellable = true;
            transactionEvent.mPlayer = &player;
            transactionEvent.mCause = name;
            transactionEvent.mItem = &item;
            transactionEvent.mAmount = (double) action.mCount;
            transactionEvent.mSourceContainer = containerName(sourceType);
            transactionEvent.mSourceSlot = action.mSource.mSlot;
            if (hasDestination(action.mType)) {
                transactionEvent.mDestinationContainer = containerName(action.mDestination.mContainerName.mContainer);
                transactionEvent.mDestinationSlot = action.mDestination.mSlot;
            }

            plugins.dispatch(transactionEvent);
            if (transactionEvent.mCancelled)
                return false;
        }
        return true;
    }

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
            owner.playLevelSound(level, LevelSoundEvent::BUNDLE_REMOVE_ONE, player.getPosition(), "minecraft:player");
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
    equipment.mBody = inventory.getBodyArmor();

    owner.sendToPlayerViewers(player, equipment, false);
}

void InventoryHandler::sendOffhandContent(ServerNetworkHandler &owner, ServerPlayer &player) {
    const ItemStack &offhand = player.getInventory().getOffhand();

    MobEquipmentPacket equipment;
    equipment.mRuntimeActorId = (int64_t) player.getRuntimeId();
    equipment.mItem = offhand;
    equipment.mInventorySlot = PlayerInventory::OFFHAND_NETWORK_SLOT;
    equipment.mHotbarSlot = PlayerInventory::OFFHAND_NETWORK_SLOT;
    equipment.mContainerId = PlayerInventory::CONTAINER_ID_OFFHAND;

    owner.sendToPlayerViewers(player, equipment, true);
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

    owner.sendToPlayerViewers(player, equipment, false);
}

void InventoryHandler::handleMobEquipment(ServerNetworkHandler &owner, ServerPlayer &player,
                                          const MobEquipmentPacket &packet) {
    if (packet.mContainerId != PlayerInventory::CONTAINER_ID_INVENTORY)
        return;

    PlayerInventory &inventory = player.getInventory();

    const int previousSlot = inventory.getSelectedSlot();

    if (packet.mHotbarSlot != previousSlot && packet.mHotbarSlot >= 0
        && packet.mHotbarSlot < PlayerInventory::HOTBAR_SIZE) {
        if (PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_PLAYER_ITEM_HELD)) {
            ItemStack next = inventory.getItem(packet.mHotbarSlot);
            PluginEvent heldEvent;
            heldEvent.mType = FALCON_EVENT_PLAYER_ITEM_HELD;
            heldEvent.mCancellable = true;
            heldEvent.mPlayer = &player;
            heldEvent.mSourceSlot = previousSlot;
            heldEvent.mDestinationSlot = packet.mHotbarSlot;
            heldEvent.mItem = &next;
            plugins->dispatch(heldEvent);
            if (heldEvent.mCancelled) {
                sendHeldItem(owner, player);
                return;
            }
        }
    }

    player.getInventoryManager().onClientSelectHotbarSlot(packet.mHotbarSlot);
    inventory.setSelectedSlot(packet.mHotbarSlot);

    if (previousSlot != inventory.getSelectedSlot()) {
        const ItemStack &held = inventory.getItemInHand();
        const std::string identifier = held.mDefinition == nullptr ? "minecraft:air"
                                                                   : held.mDefinition->getIdentifier();
        PlayerHotbarSelectedSlotChangeAfterEvent event(player, inventory.getSelectedSlot(), previousSlot, identifier);
        owner.getEventBus().after().mPlayerHotbarSelectedSlotChange.emit(event);
    }

    MobEquipmentPacket equipment;
    equipment.mRuntimeActorId = (int64_t) player.getRuntimeId();
    equipment.mItem = inventory.getItemInHand();
    equipment.mInventorySlot = inventory.getSelectedSlot();
    equipment.mHotbarSlot = inventory.getSelectedSlot();
    equipment.mContainerId = PlayerInventory::CONTAINER_ID_INVENTORY;

    owner.sendToPlayerViewers(player, equipment, false);
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
    const ItemStack armorBefore[PlayerInventory::ARMOR_SIZE] = {inventory.getArmor(0), inventory.getArmor(1),
                                                                inventory.getArmor(2), inventory.getArmor(3)};

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

    // the client named its bundles by the ids it knew: its requests wait for the new ones
    const bool renumberedBundles = BundleInventory::separateBundleIds(inventory, openContainer);

    player.getInventoryManager().loadFurnaceView();

    InventoryManager &manager = player.getInventoryManager();
    Level &level = owner.getLevelFor(player);
    const Vector3i anvilPosition = manager.getAnvilPosition();

    Level &mapLevel = MapItem::getMapLevel(owner, player);
    CartographyRecipes::Maps cartographyMaps;
    cartographyMaps.mGetMap = [&mapLevel](int64_t mapId) { return mapLevel.getMapData(mapId); };
    cartographyMaps.mExpand = [&mapLevel](int64_t mapId, bool scaled) {
        return mapLevel.getMaps().expandMap(mapLevel.getMapStorage(), mapId, scaled);
    };
    // the client's copy id from MapCreateLockedCopyPacket is ignored: it could claim an id the counter hands a
    // later map, and creating that map would replace the copy
    cartographyMaps.mAllocateLockId = [&owner, &mapLevel](int64_t) {
        int64_t copyId = owner.allocateActorUniqueId();
        while (mapLevel.getMapData(copyId) != nullptr)
            copyId = owner.allocateActorUniqueId();
        return copyId;
    };

    const bool grindstoneOpen = manager.isGrindstoneOpenIn(level) && VanillaBlocks::getAs<GrindstoneBlock>(
            level.getBlockState(anvilPosition.x, anvilPosition.y, anvilPosition.z).mName) != nullptr;
    const bool leftGrindstone = manager.isGrindstoneOpen() && (!grindstoneOpen || !withinContainerReach(
            player, Vector3f((float) anvilPosition.x + 0.5f, (float) anvilPosition.y + 0.5f,
                             (float) anvilPosition.z + 0.5f)));
    if (leftGrindstone)
        manager.onCurrentWindowRemove();

    const bool loomOpen = manager.isLoomOpenIn(level) && VanillaBlocks::getAs<LoomBlock>(
            level.getBlockState(anvilPosition.x, anvilPosition.y, anvilPosition.z).mName) != nullptr;
    const bool leftLoom = manager.isLoomOpen() && (!loomOpen || !withinContainerReach(
            player, Vector3f((float) anvilPosition.x + 0.5f, (float) anvilPosition.y + 0.5f,
                             (float) anvilPosition.z + 0.5f)));
    if (leftLoom)
        manager.onCurrentWindowRemove();

    const bool leftSmithing = manager.isSmithingOpen() && (!manager.isWorkbenchOpenIn(level) || !withinContainerReach(
            player, Vector3f((float) anvilPosition.x + 0.5f, (float) anvilPosition.y + 0.5f,
                             (float) anvilPosition.z + 0.5f)));

    const bool leftWorkbench =
            (manager.isAnvilOpen() || manager.isCartographyOpen() || manager.isStonecutterOpen())
            && (!manager.isWorkbenchOpenIn(level) || !withinContainerReach(
                    player, Vector3f((float) anvilPosition.x + 0.5f, (float) anvilPosition.y + 0.5f,
                                     (float) anvilPosition.z + 0.5f)));
    if (leftWorkbench)
        manager.onCurrentWindowRemove();

    // the mount window fills the saddle and armor slots without an interaction: their events run after
    const int64_t containerActorId = manager.getContainerActorId();
    MobActor *mount = dynamic_cast<MobActor *>(containerActorId == -1 ? nullptr : owner.getActor(containerActorId));
    const std::vector<ItemStack> mountEquipment = mount == nullptr ? std::vector<ItemStack>()
                                                                   : mount->getEquippableItems();

    MobActor *trader = manager.isTradeOpen() ? dynamic_cast<MobActor *>(owner.getActor(manager.getTradeActorId()))
                                             : nullptr;
    if (trader != nullptr && trader->getEconomy().getTradingPlayer() != player.getUniqueId())
        trader = nullptr;

    // the same bound as opening the screen: a trader left behind or in another world closes it
    const bool leftTrade = trader != nullptr
                           && (!trader->sharesLevelWith(player)
                               || trader->distanceSquaredTo(player) > ENTITY_INTERACT_REACH * ENTITY_INTERACT_REACH);
    if (leftTrade) {
        manager.onCurrentWindowRemove();
        trader = nullptr;
    }

    const bool leftContainer = leftGrindstone || leftLoom || leftSmithing || leftWorkbench || leftTrade
                               || hasLeftOpenContainer(owner, player);

    BeaconBlockActor *beacon = nullptr;
    if (manager.isContainerOpen() && manager.getCurrentWindowType() == ContainerType::Beacon
        && containerActorId == -1)
        beacon = level.getBlockActors().find<BeaconBlockActor>(manager.getContainerPosition());

    for (const ItemStackRequest &request: packet.mRequests) {
        // nothing to run: the same answer as a request run on a copy of the inventory, without the copy
        if (request.mActions.empty() && !renumberedBundles && !leftContainer) {
            ItemStackResponseEntry done;
            done.mRequestId = request.mRequestId;
            done.mResult = ItemStackRequestHandler::RESULT_OK;
            response.mEntries.push_back(std::move(done));
            continue;
        }

        const bool anvilOpen = manager.isAnvilOpen()
                               && AnvilBlock::matches(
                                       level.getBlockState(anvilPosition.x, anvilPosition.y, anvilPosition.z));
        const int32_t gameType = player.getGameType();
        const bool creativeMode = gameType == (int32_t) GameType::Creative || gameType == (int32_t) GameType::Spectator;
        int32_t enchantLevelsConsumed = 0;
        int32_t anvilLevelsConsumed = 0;
        bool anvilUsed = false;
        int32_t grindstoneXpCost = -1;
        BeaconChoice beaconChoice;
        CartographyLock cartographyLock;
        CartographyExpansion cartographyExpansion;
        const bool cartographyOpen = manager.isCartographyOpen()
                                     && level.getBlockState(anvilPosition.x, anvilPosition.y, anvilPosition.z).mName
                                                == CartographyTableBlock::IDENTIFIER;
        const bool stonecutterOpen =
                manager.isStonecutterOpen()
                && StonecutterBlock::matches(
                        level.getBlockState(anvilPosition.x, anvilPosition.y, anvilPosition.z).mName);

        const bool smithingOpen = manager.isSmithingOpen() && VanillaBlocks::getAs<SmithingTableBlock>(
                level.getBlockState(anvilPosition.x, anvilPosition.y, anvilPosition.z).mName) != nullptr;

        // creative only: the spectator shares the creative flag below but has no creative inventory
        bool creativeCraft = false;
        for (const ItemStackRequestAction &action: request.mActions)
            creativeCraft = creativeCraft || action.mType == ItemStackRequestActionType::CraftCreative;

        // the enchanting slots exist only while the table's window is open
        bool strayEnchanting = false;
        if (!manager.isContainerOpen() || manager.getCurrentWindowType() != ContainerType::Enchantment) {
            for (const ItemStackRequestAction &action: request.mActions)
                strayEnchanting = strayEnchanting || usesEnchantingTable(action);
        }

        // with doLimitedCrafting a locked recipe does not craft
        bool lockedRecipe = false;
        for (const ItemStackRequestAction &action: request.mActions) {
            if (action.mType == ItemStackRequestActionType::CraftRecipe
                || action.mType == ItemStackRequestActionType::CraftRecipeAuto)
                lockedRecipe = lockedRecipe || !RecipeUnlockSystem::mayCraft(owner, player, action.mRecipeNetworkId);
        }

        if (leftContainer || renumberedBundles || strayEnchanting || lockedRecipe
            || takesForeignEnchant(manager, inventory, request)
            || (creativeCraft && gameType != (int32_t) GameType::Creative)
            || !allowRequest(owner, player, request)) {
            ItemStackResponseEntry rejected;
            rejected.mRequestId = request.mRequestId;
            rejected.mResult = ItemStackRequestHandler::RESULT_ERROR;
            needsResync = true;
            response.mEntries.push_back(std::move(rejected));
            continue;
        }

        ItemStack craftedRecipeItem = ItemStack::air();
        TradeCraft trade;
        MerchantOffers *tradeOffers = trader == nullptr ? nullptr : trader->getEconomy().getOffers(owner, *trader);
        if (tradeOffers != nullptr)
            tradeOffers->mUnlockedTier = trader->getEconomy().getTier();
        ItemStackResponseEntry entry = ItemStackRequestHandler::execute(
                inventory, request, owner.getCreativeItems(), owner.getRecipeOutputs(), owner.getRecipeSourceIndices(),
                player.getInventoryManager().isCraftingTableOpen(), player.getInventoryManager().isFurnaceOpen(),
                player.getInventoryManager().getContainer(), &droppedItems, &owner.getCodecContext(), &bundles,
                player.getExperience().getXpLevel(), creativeMode, &enchantLevelsConsumed, anvilOpen,
                &anvilLevelsConsumed, &anvilUsed, cartographyOpen ? &cartographyMaps : nullptr, &cartographyLock,
                stonecutterOpen ? &owner.getStonecutterRecipeIndices() : nullptr, &craftedRecipeItem, tradeOffers,
                &trade, smithingOpen ? &owner.getCachedCraftingData() : nullptr,
                player.getCodec() ? player.getCodec()->getProtocolVersion() : 0,
                grindstoneOpen && !leftGrindstone && gameType != (int32_t) GameType::Spectator, &grindstoneXpCost,
                beacon == nullptr ? -1 : beacon->getLevels(), &beaconChoice,
                loomOpen && !leftLoom && gameType != (int32_t) GameType::Spectator,
                cartographyOpen ? nullptr : &cartographyMaps, &cartographyExpansion, &player);

        if (entry.mResult == ItemStackRequestHandler::RESULT_OK)
            CartographyRecipes::applyExpansion(cartographyExpansion, cartographyMaps);

        if (entry.mResult == ItemStackRequestHandler::RESULT_OK && beacon != nullptr && beaconChoice.mPaid) {
            beacon->applyPayment(beaconChoice.mPrimary, beaconChoice.mSecondary);
            BlockActionHandler::broadcastBlockActorData(owner, level, *beacon);
        }

        if (entry.mResult == ItemStackRequestHandler::RESULT_OK && grindstoneXpCost >= 0) {
            const Vector3f at((float) anvilPosition.x + 0.5f, (float) anvilPosition.y + 0.5f,
                              (float) anvilPosition.z + 0.5f);
            owner.playLevelSound(level, "block.grindstone.use", at);
            if (grindstoneXpCost > 0) {
                const int32_t minimum = (grindstoneXpCost + 1) / 2;
                static thread_local std::mt19937 random(std::random_device{}());
                const int32_t xp = minimum + std::uniform_int_distribution<int32_t>(0, minimum - 1)(random);
                owner.spawnExperienceOrbs(level, at, xp);
            }
        }

        if (entry.mResult == ItemStackRequestHandler::RESULT_OK && trader != nullptr && trade.mOfferIndex >= 0)
            trader->getEconomy().onTrade(owner, *trader, trade.mOfferIndex, trade.mCrafts);

        // the copy waits for the request to hold: a rejected one would leave an orphan map behind
        if (entry.mResult == ItemStackRequestHandler::RESULT_OK
            && cartographyLock.mCopyId != MapItemSavedData::INVALID_ID
            && mapLevel.getMaps().copyAndLockMap(mapLevel.getMapStorage(), cartographyLock.mOriginalId,
                                                 cartographyLock.mCopyId)
                       != nullptr) {
            // the client's item still names the id it drew: the response carries no tag
            if (manager.getClientMapLock().mCopyId != cartographyLock.mCopyId)
                needsResync = true;
            manager.setClientMapLock(CartographyLock());
        }

        // the item is enchanted now: its other options go with it, the next ones are rolled for what it became
        if (entry.mResult == ItemStackRequestHandler::RESULT_OK && craftsEnchant(request))
            manager.clearEnchantOptions();

        if (entry.mResult == ItemStackRequestHandler::RESULT_OK && enchantLevelsConsumed > 0) {
            player.getExperience().subtractXpLevels(enchantLevelsConsumed);
            player.syncExperience();
            owner._sendAttributes(player);
        }

        if (entry.mResult == ItemStackRequestHandler::RESULT_OK && anvilUsed) {
            if (anvilLevelsConsumed > 0) {
                player.getExperience().subtractXpLevels(anvilLevelsConsumed);
                player.syncExperience();
                owner._sendAttributes(player);
            }
            AnvilBlock::onUsed(owner, level, anvilPosition, gameType == (int32_t) GameType::Creative);
            if (PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_ANVIL_USE)) {
                PluginEvent anvilEvent;
                anvilEvent.mType = FALCON_EVENT_ANVIL_USE;
                anvilEvent.mPlayer = &player;
                anvilEvent.mLevel = &level;
                anvilEvent.mBlockPosition = anvilPosition;
                anvilEvent.mAmount = (double) anvilLevelsConsumed;
                plugins->dispatch(anvilEvent);
            }
        }

        if (entry.mResult != ItemStackRequestHandler::RESULT_OK) {
            needsResync = true;
            playBundleSounds(owner, player, request, false);
        } else {
            playBundleSounds(owner, player, request, true);
            if (!craftedRecipeItem.isAir()) {
                const InventoryManager &invManager = player.getInventoryManager();
                const Vector3i tablePosition = smithingOpen ? anvilPosition : invManager.getCraftingTablePosition();
                if (smithingOpen)
                    owner.playLevelSound(level, "smithing_table.use", Vector3f((float) tablePosition.x + 0.5f,
                                         (float) tablePosition.y + 0.5f, (float) tablePosition.z + 0.5f));
                owner.getScriptEngine().onPlayerCraftRecipe(
                        player, craftedRecipeItem,
                        smithingOpen || invManager.isCraftingTableOpen() ? &tablePosition : nullptr);
            }
        }

        response.mEntries.push_back(std::move(entry));
    }

    player.getInventoryManager().storeFurnaceView(owner);
    PluginEvents::armorChanged(player, armorBefore);

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

    for (const ItemStack &dropped: droppedItems) {
        ItemStack droppedCopy = dropped;
        PluginEvent dropEvent;
        dropEvent.mType = FALCON_EVENT_PLAYER_DROP_ITEM;
        dropEvent.mCancellable = true;
        dropEvent.mPlayer = &player;
        dropEvent.mItem = &droppedCopy;
        PluginManager::getInstance().dispatch(dropEvent);

        if (!dropEvent.mCancelled) {
            owner._throwItem(player, dropped);
            continue;
        }

        std::vector<int> touched;
        const int remaining = inventory.addItemPartial(dropped, touched);
        needsResync = true;
        if (remaining <= 0)
            continue;

        ItemStack rest = dropped;
        rest.mCount = remaining;
        owner._throwItem(player, rest);
    }

    player.getInventoryManager().syncCraftingPreview(owner.getRecipeOutputs(), owner.getRecipeSourceIndices());
    if (player.getInventoryManager().isFurnaceOpen()) {
        player.getInventoryManager().syncContents(InventoryManager::InventoryId::FurnaceInput);
    }

    _updateEnchantOptions(owner, player);

    const ItemStack &offhandAfter = inventory.getOffhand();
    if (offhandAfter.mDefinition != offhandBefore.mDefinition || offhandAfter.mCount != offhandBefore.mCount
        || offhandAfter.mDamage != offhandBefore.mDamage || !(offhandAfter.mTag == offhandBefore.mTag))
        sendOffhandContent(owner, player);

    if (mount != nullptr)
        mount->onEquippableItemsChanged(owner, mountEquipment, &player);

    if (player.getInventoryManager().getContainerActorId() != -1)
        owner.refreshEntityContainerViewers(player.getInventoryManager().getContainerActorId(), &player);
    else if (player.getInventoryManager().isContainerOpen())
        owner.refreshContainerViewers(player.getInventoryManager().getContainerPosition(), &player);

    if (response.mEntries.empty())
        return;

    owner.getNetworkHandler().send(id, response, owner.getCodecContext());

    if (needsResync) {
        owner._sendInventory(player);
        manager.syncSmithingInputs();
        manager.syncGrindstoneInputs();
        manager.syncLoomInputs();
    }
    if (!leftContainer && (needsResync || renumberedBundles)) {
        manager.refreshOpenContainer(manager.getContainerPosition());
        manager.refreshEntityContainer(manager.getContainerActorId());
    }
    if (renumberedBundles)
        manager.syncBundles();
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
        packet.mOptions = EnchantmentHelper::getEnchantOptions(
                owner.getLevelFor(player), manager.getContainerPosition(), current, manager.getEnchantSeed());

    if (PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_PLAYER_ENCHANTING_OPTIONS)) {
        ItemStack proposedInput = current;
        const ItemStack *material = player.getInventory().resolveSlot(ContainerSlotType::EnchantingMaterial, 0);
        ItemStack lapis = material == nullptr ? empty : *material;
        PluginEvent event;
        event.mType = FALCON_EVENT_PLAYER_ENCHANTING_OPTIONS;
        event.mCancellable = true;
        event.mPlayer = &player;
        event.mLevel = &owner.getLevelFor(player);
        event.mBlockPosition = manager.getContainerPosition();
        event.mItem = &proposedInput;
        event.mResult = &lapis;
        const std::string original = encodeEnchantOptions(packet.mOptions);
        event.mCustomData = original;
        plugins->dispatch(event);

        std::vector<EnchantOptionData> modified;
        if (event.mCancelled
            || (event.mCustomData != original && decodeEnchantOptions(event.mCustomData, modified))) {
            std::vector<int32_t> obsolete;
            for (const auto &option: packet.mOptions)
                obsolete.push_back(option.mEnchantNetId);
            EnchantmentHelper::discardOptions(obsolete);
            packet.mOptions = event.mCancelled ? std::vector<EnchantOptionData>() : std::move(modified);
            EnchantmentHelper::registerOptions(packet.mOptions);
        }
    }

    std::vector<int32_t> offered;
    for (const EnchantOptionData &option: packet.mOptions)
        offered.push_back(option.mEnchantNetId);
    manager.setEnchantOptions(std::move(offered));

    owner.getNetworkHandler().send(player.getNetworkIdentifier(), packet, owner.getCodecContext());
}

bool InventoryHandler::interactWithEntity(ServerNetworkHandler &owner, ServerPlayer &player, uint64_t runtimeId,
                                          const Vector3f &clickPosition) {
    ServerActor *target = owner.getActorByRuntimeId(runtimeId);
    // the click was predicted: the actor may be gone since
    if (target == nullptr) {
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                              player.getInventory().getSelectedSlot());
        return false;
    }

    // the id alone names any actor of the server: same level and within reach only
    const Vector3f delta(target->getPosition().x - player.getPosition().x,
                         target->getPosition().y - player.getPosition().y,
                         target->getPosition().z - player.getPosition().z);
    if (!target->sharesLevelWith(player)
        || !(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z
             <= ENTITY_INTERACT_REACH * ENTITY_INTERACT_REACH))
        return false;

    if (owner.getScriptEngine().beforePlayerInteractWithEntity(player, *target))
        return false;
    target = owner.getActorByRuntimeId(runtimeId);
    if (target == nullptr)
        return false;

    PluginEvent event;
    event.mType = FALCON_EVENT_PLAYER_INTERACT_ENTITY;
    event.mCancellable = true;
    event.mPlayer = &player;
    event.mEntity = target;
    PluginManager::getInstance().dispatch(event);
    if (event.mCancelled)
        return false;

    target = owner.getActorByRuntimeId(runtimeId);
    // a sneaking click opens a tamed mount's inventory instead of riding it
    if (target != nullptr && dynamic_cast<MobActor *>(target) != nullptr && player.getFlags().get(ActorFlag::Sneaking)
        && target->getEntityContainer() != nullptr
        && player.getInventoryManager().openEntityContainer(*target, ContainerType::Horse))
        return true;
    if (target == nullptr)
        return false;
    const ItemStack before = player.getInventory().getItemInHand();
    if (!target->onInteractAt(owner, player, clickPosition))
        return false;
    target = owner.getActorByRuntimeId(runtimeId);
    if (target != nullptr)
        owner.getScriptEngine().onPlayerInteractWithEntity(player, *target, before);
    return true;
}

void InventoryHandler::handleTransaction(ServerNetworkHandler &owner, ServerPlayer &player,
                                         const InventoryTransactionPacket &packet) {
    if (packet.mTransactionType == InventoryTransactionType::ItemUseOnEntity) {
        if (packet.mHotbarSlot != player.getInventory().getSelectedSlot()) {
            owner._sendInventory(player);
            return;
        }

        if (packet.mActionType == 1 || packet.mActionType == 2) {
            player.attackActor(owner, (uint64_t) packet.mRuntimeActorId, &packet.mClickPosition);
            return;
        }

        if (packet.mActionType == 0)
            interactWithEntity(owner, player, (uint64_t) packet.mRuntimeActorId, packet.mClickPosition);
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
        owner._keepItemUse(player);
        const bool wasUsing = player.getFlags().get(ActorFlag::UsingItem);

        const ItemStack &releasedItem = player.getInventory().getItemInHand();
        if (wasUsing && !releasedItem.isAir() && releasedItem.mDefinition != nullptr) {
            const Item *itemType = VanillaItems::fromIdentifier(releasedItem.mDefinition->getIdentifier());
            const int32_t elapsedTicks = (int32_t) (owner.getCurrentTick() - player.getItemUseStartTick());
            const ItemStack released = releasedItem;
            const int32_t ticksLeft = owner.getItemUseTicksLeft(player);
            const CustomItemDefinition *pack =
                    releasedItem.mDefinition->isComponentBased()
                            ? CustomContentRegistry::getInstance().findItem(releasedItem.mDefinition->getIdentifier())
                            : nullptr;
            if ((itemType != nullptr && itemType->onStopUsing(owner, player, releasedItem, elapsedTicks))
                || (pack != nullptr && PackShooter::release(owner, player, *pack, elapsedTicks))) {
                player.getFlags().set(ActorFlag::UsingItem, false);
                owner._sendEntityData(player);
                owner.getScriptEngine().onItemStopUse(player, released, ticksLeft, true);
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
                owner.getScriptEngine().onItemStopUse(player, player.getInventory().getItemInHand(),
                                                      owner.getItemUseTicksLeft(player), true);
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

    // the item stack request path refuses to drop a locked item: this older path must too
    if (ItemProperties::getLockMode(*item) != ItemLockMode::None) {
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, sourceSlot);
        return;
    }

    ItemStack thrown = *item;
    thrown.mCount = droppedCount;
    thrown.mUsingNetId = false;
    thrown.mNetId = 0;

    ItemStack eventItem = thrown;
    PluginEvent dropEvent;
    dropEvent.mType = FALCON_EVENT_PLAYER_DROP_ITEM;
    dropEvent.mCancellable = true;
    dropEvent.mPlayer = &player;
    dropEvent.mItem = &eventItem;
    PluginManager::getInstance().dispatch(dropEvent);
    if (dropEvent.mCancelled) {
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, sourceSlot);
        return;
    }

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
    manager.dispatchPluginClose();
    int windowId = (int) packet.mWindowId;
    if (windowId == InventoryManager::CONTAINER_ID_NONE) {
        windowId = manager.getCurrentWindowId();
    }
    const bool craftingTable = manager.isCraftingTableOpen() && windowId == manager.getCraftingTableWindowId();
    const bool furnace = manager.isFurnaceOpen() && windowId == manager.getFurnaceWindowId();
    const int gridSize = craftingTable ? CraftingTableInventory::SIZE
                         : furnace     ? FurnaceInventory::SIZE
                                       : PlayerInventory::CRAFTING_SIZE;

    for (int slot = 0; slot < gridSize; slot++) {
        if (furnace) {
            continue;
        }
        ItemStack item = craftingTable ? inventory.getCraftingTableItem(slot)
                         : furnace     ? inventory.getFurnaceItem(slot)
                                       : inventory.getCraftingItem(slot);
        if (item.isAir()) {
            continue;
        }

        returnToInventory(player, item);
        if (craftingTable) {
            inventory.setCraftingTableItem(slot, std::move(item));
        } else if (furnace) {
            inventory.setFurnaceItem(slot, std::move(item));
        } else {
            inventory.setCraftingItem(slot, std::move(item));
        }

        manager.syncSlot(craftingTable ? InventoryManager::InventoryId::CraftingTableInput
                         : furnace     ? InventoryManager::InventoryId::FurnaceInput
                                       : InventoryManager::InventoryId::CraftingInput,
                         slot);
    }

    if (!inventory.getCursor().isAir()) {
        ItemStack cursor = inventory.getCursor();
        returnToInventory(player, cursor);
        inventory.setCursor(std::move(cursor));
        manager.syncSlot(InventoryManager::InventoryId::Cursor, 0);
    }

    if (returnScreenItems(player))
        manager.syncAll();

    manager.onClientRemoveWindow(windowId);
}

void InventoryHandler::resetInventory(ServerNetworkHandler &owner, ServerPlayer &player) {
    PlayerInventory &inventory = player.getInventory();
    std::vector<ItemStack> returned;

    for (int slot = 0; slot < PlayerInventory::CRAFTING_SIZE; ++slot) {
        if (!inventory.getCraftingItem(slot).isAir())
            returned.push_back(inventory.getCraftingItem(slot));
        inventory.setCraftingItem(slot, ItemStack::air());
    }

    for (int slot = 0; slot < PlayerInventory::CRAFTING_TABLE_SIZE; ++slot) {
        if (!inventory.getCraftingTableItem(slot).isAir())
            returned.push_back(inventory.getCraftingTableItem(slot));
        inventory.setCraftingTableItem(slot, ItemStack::air());
    }

    // the anvil keeps nothing: leaving with it open returns the input and material
    for (int slot = 0; slot < PlayerInventory::ANVIL_SIZE; ++slot) {
        if (!inventory.getAnvilItem(slot).isAir())
            returned.push_back(inventory.getAnvilItem(slot));
        inventory.setAnvilItem(slot, ItemStack::air());
    }

    for (int slot = 0; slot < PlayerInventory::CARTOGRAPHY_SIZE; ++slot) {
        if (!inventory.getCartographyItem(slot).isAir())
            returned.push_back(inventory.getCartographyItem(slot));
        inventory.setCartographyItem(slot, ItemStack::air());
    }

    for (int index = 0; index < PlayerInventory::SMITHING_SIZE; ++index) {
        const ItemStack &item = inventory.getSmithingItem(index);
        if (!item.isAir())
            returned.push_back(item);
        inventory.setSmithingItem(index, ItemStack::air());
    }

    for (int index = 0; index < PlayerInventory::GRINDSTONE_SIZE; ++index) {
        const ItemStack &item = inventory.getGrindstoneItem(index);
        if (!item.isAir())
            returned.push_back(item);
        inventory.setGrindstoneItem(index, ItemStack::air());
    }

    for (int index = 0; index < PlayerInventory::LOOM_SIZE; ++index) {
        const ItemStack &item = inventory.getLoomItem(index);
        if (!item.isAir())
            returned.push_back(item);
        inventory.setLoomItem(index, ItemStack::air());
    }

    if (!inventory.getStonecutterInput().isAir())
        returned.push_back(inventory.getStonecutterInput());
    inventory.setStonecutterInput(ItemStack::air());

    if (!inventory.getBeaconPayment().isAir())
        returned.push_back(inventory.getBeaconPayment());
    inventory.setBeaconPayment(ItemStack::air());

    if (!inventory.getCursor().isAir())
        returned.push_back(inventory.getCursor());
    inventory.setCursor(ItemStack::air());

    for (const ContainerSlotType type: {ContainerSlotType::EnchantingInput, ContainerSlotType::EnchantingMaterial}) {
        ItemStack *slot = inventory.resolveSlot(type, 0);
        if (slot == nullptr || slot->isAir())
            continue;

        returned.push_back(*slot);
        *slot = ItemStack::air();
    }

    for (ItemStack &item: returned) {
        returnToInventory(player, item);
        if (item.mCount > 0)
            owner._throwItem(player, item);
    }
}

void InventoryHandler::handleCraftingEvent(ServerNetworkHandler &owner, ServerPlayer &player,
                                           const CraftingEventPacket &packet) {
    const bool table = packet.mType == CraftingType::Workbench;
    if (table && !player.getInventoryManager().isCraftingTableOpen()) {
        return;
    }

    const int gridWidth = table ? 3 : 2;
    const PlayerInventory &inventory = player.getInventory();
    const std::vector<ItemStack> &grid = table ? inventory.getCraftingTableContents() : inventory.getCraftingContents();
    if (packet.mOutputs.empty()) {
        return;
    }

    CraftingRecipeMatch match;
    ItemStack output = ItemStack::air();
    const std::vector<ItemStack> &recipeOutputs = owner.getRecipeOutputs();
    const std::vector<uint32_t> &recipeSourceIndices = owner.getRecipeSourceIndices();
    if (!CraftingManager::matchResult(grid, gridWidth, packet.mOutputs.front(), recipeOutputs, recipeSourceIndices,
                                      match, output)) {
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
