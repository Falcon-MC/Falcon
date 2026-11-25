#include "Falcon/Plugin/PluginBusEvents.h"

#include "Core/Event/EventBus.h"
#include "Core/Json/Json.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Network/Crypto/Base64.h"
#include "Protocol/Types/ContainerSlotType.h"
#include "Protocol/Types/ItemStack.h"
#include "Protocol/Types/ItemStackRequest.h"
#include "Protocol/Types/ItemStackRequestAction.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/Container.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Item/EnchantmentData.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <string>
#include <vector>

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

    bool isBlockContainer(ContainerSlotType type) {
        return type == ContainerSlotType::LevelEntity || type == ContainerSlotType::Barrel
               || type == ContainerSlotType::ShulkerBox || type == ContainerSlotType::CrafterBlockContainer;
    }

    std::string containerName(ContainerSlotType type) {
        if (isBlockContainer(type))
            return "container";

        switch (type) {
            // plugins see a mount's equipment and chest as one container, numbered by containerSlot
            case ContainerSlotType::HorseEquip:
                return "container";
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

    // the client numbers a mount's chest after its own equipment range; plugins get the container index
    int containerSlot(const Container *openContainer, ContainerSlotType type, int slot) {
        if (openContainer == nullptr || openContainer->getEquipmentSlotCount() == 0
            || (!isBlockContainer(type) && type != ContainerSlotType::HorseEquip))
            return slot;
        const int index = openContainer->getNetworkSlotIndex(type, slot);
        return index < 0 ? slot : index;
    }

    ItemStack itemAt(const PlayerInventory &inventory, Container *openContainer, ContainerSlotType type, int slot) {
        if (isBlockContainer(type) || type == ContainerSlotType::HorseEquip) {
            const int index = openContainer == nullptr ? -1 : openContainer->getNetworkSlotIndex(type, slot);
            return index < 0 ? ItemStack::air() : openContainer->getContainerItem(index);
        }

        const ItemStack *item = inventory.resolveSlot(type, slot);
        return item == nullptr ? ItemStack::air() : *item;
    }

    // one plugin event per action; a cancelled one stops the rest
    void dispatchTransactions(PluginManager &plugins, InventoryRequestBeforeEvent &source) {
        const ItemStackRequest &request = source.mRequest;
        const PlayerInventory &inventory = source.mPlayer.getInventory();
        Container *openContainer = source.mPlayer.getInventoryManager().getContainer();
        std::string actionsJson;
        size_t actionCount = 0;
        if (request.mActions.size() > 1) {
            for (const ItemStackRequestAction &action: request.mActions) {
                const char *name = transactionName(action.mType);
                if (name == nullptr)
                    continue;
                const bool destination = hasDestination(action.mType);
                actionsJson += actionCount++ == 0 ? "[" : ",";
                actionsJson += "[\"" + std::string(name) + "\",\""
                               + containerName(action.mSource.mContainerName.mContainer) + "\","
                               + std::to_string(containerSlot(openContainer, action.mSource.mContainerName.mContainer,
                                                              action.mSource.mSlot))
                               + ",\""
                               + (destination ? containerName(action.mDestination.mContainerName.mContainer) : "")
                               + "\","
                               + std::to_string(destination
                                                        ? containerSlot(openContainer,
                                                                        action.mDestination.mContainerName.mContainer,
                                                                        action.mDestination.mSlot)
                                                        : -1)
                               + "," + std::to_string(action.mCount) + "]";
            }
            if (actionCount > 1)
                actionsJson += "]";
        }

        for (const ItemStackRequestAction &action: request.mActions) {
            const char *name = transactionName(action.mType);
            if (name == nullptr)
                continue;

            const ContainerSlotType sourceType = action.mSource.mContainerName.mContainer;
            ItemStack item = itemAt(inventory, openContainer, sourceType, action.mSource.mSlot);
            PluginEvent event;
            event.mType = FALCON_EVENT_INVENTORY_TRANSACTION;
            event.mCancellable = true;
            event.mCancelled = source.isCancelled();
            event.mPlayer = &source.mPlayer;
            event.mCause = name;
            event.mItem = &item;
            event.mAmount = (double) action.mCount;
            event.mSourceContainer = containerName(sourceType);
            event.mSourceSlot = containerSlot(openContainer, sourceType, action.mSource.mSlot);
            if (hasDestination(action.mType)) {
                const ContainerSlotType destinationType = action.mDestination.mContainerName.mContainer;
                event.mDestinationContainer = containerName(destinationType);
                event.mDestinationSlot = containerSlot(openContainer, destinationType, action.mDestination.mSlot);
            }

            // the first of a group carries the whole request; each callback still gets its own action
            if (actionCount > 1) {
                event.mCustomData = std::move(actionsJson);
                actionsJson = "[]";
            }
            plugins.dispatch(event);
            source.setCancelled(event.mCancelled);
            if (event.mCancelled)
                return;
        }
    }
}

void PluginBusEvents::hookInventory(PluginManager &plugins, EventBus &bus) {
    InventoryBeforeSignals &before = bus.before();
    InventoryAfterSignals &after = bus.after();

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_FURNACE_BURN), before.mFurnaceBurn,
                    [&plugins](FurnaceBurnBeforeEvent &source) {
                        ItemStack burning = source.mFuel;
                        PluginEvent event;
                        event.mType = FALCON_EVENT_FURNACE_BURN;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mLevel = &source.mLevel;
                        event.mBlockPosition = source.mBlockPosition;
                        event.mItem = &burning;
                        event.mAmount = (double) source.mBurnTime;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                        if (!event.mCancelled)
                            source.mBurnTime = std::max(0, (int) event.mAmount);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_FURNACE_SMELT), before.mFurnaceSmelt,
                    [&plugins](FurnaceSmeltBeforeEvent &source) {
                        ItemStack input = source.mInput;
                        PluginEvent event;
                        event.mType = FALCON_EVENT_FURNACE_SMELT;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mLevel = &source.mLevel;
                        event.mBlockPosition = source.mBlockPosition;
                        event.mItem = &input;
                        event.mResult = &source.mResult;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    // a close listener needs it too: the manager closes only a container it saw open
    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_INVENTORY_OPEN)
                            || plugins.hasSubscribers(FALCON_EVENT_INVENTORY_CLOSE),
                    before.mBlockContainerOpen, [&plugins](BlockContainerOpenBeforeEvent &source) {
                        const Vector3i &position = source.mBlockPosition;
                        PluginEvent event;
                        event.mType = FALCON_EVENT_INVENTORY_OPEN;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mLevel = &source.mLevel;
                        event.mBlockPosition = position;
                        event.mBlockName = source.mLevel.getBlockState(position.x, position.y, position.z).mName;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_INVENTORY_CLOSE), after.mBlockContainerClose,
                    [&plugins](BlockContainerCloseAfterEvent &source) {
                        const Vector3i &position = source.mBlockPosition;
                        PluginEvent event;
                        event.mType = FALCON_EVENT_INVENTORY_CLOSE;
                        event.mPlayer = &source.mPlayer;
                        event.mLevel = &source.mLevel;
                        event.mBlockPosition = position;
                        event.mBlockName = source.mLevel.getBlockState(position.x, position.y, position.z).mName;
                        plugins.dispatch(event);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_INVENTORY_TRANSACTION), before.mInventoryRequest,
                    [&plugins](InventoryRequestBeforeEvent &source) { dispatchTransactions(plugins, source); });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_ITEM_HELD), before.mPlayerItemHeld,
                    [&plugins](PlayerItemHeldBeforeEvent &source) {
                        ItemStack next = source.mItem;
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_ITEM_HELD;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mSourceSlot = source.mPreviousSlot;
                        event.mDestinationSlot = source.mSlot;
                        event.mItem = &next;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_DROP_ITEM), before.mPlayerDropItem,
                    [&plugins](PlayerDropItemBeforeEvent &source) {
                        ItemStack dropped = source.mItem;
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_DROP_ITEM;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mItem = &dropped;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_ENCHANTING_OPTIONS), before.mEnchantingOptions,
                    [&plugins](EnchantingOptionsBeforeEvent &source) {
                        ItemStack proposedInput = source.mInput;
                        ItemStack lapis = source.mMaterial;
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_ENCHANTING_OPTIONS;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mLevel = &source.mLevel;
                        event.mBlockPosition = source.mBlockPosition;
                        event.mItem = &proposedInput;
                        event.mResult = &lapis;
                        const std::string original = encodeEnchantOptions(source.mOptions);
                        event.mCustomData = original;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                        if (event.mCancelled || event.mCustomData == original)
                            return;

                        std::vector<EnchantOptionData> modified;
                        if (decodeEnchantOptions(event.mCustomData, modified)) {
                            source.mReplacement = std::move(modified);
                            source.mReplaced = true;
                        }
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ANVIL_USE), after.mAnvilUse,
                    [&plugins](AnvilUseAfterEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_ANVIL_USE;
                        event.mPlayer = &source.mPlayer;
                        event.mLevel = &source.mLevel;
                        event.mBlockPosition = source.mBlockPosition;
                        event.mAmount = (double) source.mLevelsConsumed;
                        plugins.dispatch(event);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_ARMOR_CHANGE), after.mPlayerArmorChange,
                    [&plugins](PlayerArmorChangeAfterEvent &source) {
                        for (int slot = 0; slot < PlayerInventory::ARMOR_SIZE; ++slot) {
                            ItemStack before = source.mPrevious[slot];
                            ItemStack after = source.mPlayer.getInventory().getArmor(slot);
                            if (before.isAir() && after.isAir())
                                continue;
                            if (!before.isAir() && !after.isAir() && before.mDefinition == after.mDefinition
                                && before.mCount == after.mCount && before.mDamage == after.mDamage
                                && before.mTag == after.mTag)
                                continue;

                            PluginEvent event;
                            event.mType = FALCON_EVENT_PLAYER_ARMOR_CHANGE;
                            event.mPlayer = &source.mPlayer;
                            event.mDestinationSlot = slot;
                            event.mItem = &after;
                            event.mResult = &before;
                            plugins.dispatch(event);
                        }
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_INTERACT_ENTITY), before.mPlayerInteractEntity,
                    [&plugins](PlayerInteractEntityBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_INTERACT_ENTITY;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mEntity = &source.mTarget;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_PICKUP_ITEM), before.mPlayerPickupItem,
                    [&plugins](PlayerPickupItemBeforeEvent &source) {
                        ItemStack picked = source.mItem;
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_PICKUP_ITEM;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mEntity = &source.mItemActor;
                        event.mItem = &picked;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ITEM_MERGE), before.mItemMerge,
                    [&plugins](ItemMergeBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_ITEM_MERGE;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mEntity = &source.mMerged;
                        event.mTarget = &source.mTarget;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ITEM_DESPAWN), before.mItemDespawn,
                    [&plugins](ItemDespawnBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_ITEM_DESPAWN;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mEntity = &source.mItemActor;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_ITEM_USE), before.mPlayerItemUse,
                    [&plugins](PlayerItemUseBeforeEvent &source) {
                        ItemStack used = source.mItem;
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_ITEM_USE;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mItem = &used;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_ITEM_CONSUME), before.mPlayerItemConsume,
                    [&plugins](PlayerItemConsumeBeforeEvent &source) {
                        ItemStack consumed = source.mItem;
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_ITEM_CONSUME;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mItem = &consumed;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_EDIT_BOOK), before.mPlayerEditBook,
                    [&plugins](PlayerEditBookBeforeEvent &source) {
                        ItemStack original = source.mBook;
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_EDIT_BOOK;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mItem = &original;
                        event.mResult = &source.mResult;
                        event.mAmount = (double) source.mAction;
                        event.mChunkX = source.mPage;
                        event.mChunkZ = source.mSecondaryPage;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_CRAFT_ITEM), before.mCraftItem,
                    [&plugins](CraftItemBeforeEvent &source) {
                        ItemStack output = source.mOutput;
                        PluginEvent event;
                        event.mType = FALCON_EVENT_CRAFT_ITEM;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mItem = &output;
                        event.mResult = &output;
                        event.mAmount = source.mRepetitions;
                        event.mPreviousAmount = source.mRecipeId;
                        event.mDrops = &source.mInputs;
                        event.mCustomData = "[" + std::to_string(source.mGridWidth) + ","
                                            + std::to_string(source.mRecipeWidth) + ",[";
                        bool first = true;
                        for (const auto &[slot, count]: source.mSlots) {
                            if (!first)
                                event.mCustomData += ',';
                            first = false;
                            event.mCustomData += '[' + std::to_string(slot) + ',' + std::to_string(count) + ']';
                        }
                        event.mCustomData += "]]";
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ITEM_ENCHANT), before.mItemEnchant,
                    [&plugins](ItemEnchantBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_ITEM_ENCHANT;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mItem = &source.mInput;
                        event.mResult = &source.mResult;
                        event.mAmount = source.mCost;
                        event.mPreviousAmount = source.mRequiredLevel;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ITEM_DAMAGE), before.mItemDamage,
                    [&plugins](ItemDamageBeforeEvent &source) {
                        ItemStack damaged = source.mItem;
                        PluginEvent event;
                        event.mType = FALCON_EVENT_ITEM_DAMAGE;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = source.mPlayer;
                        event.mItem = &damaged;
                        event.mAmount = (double) source.mDamage;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                        if (!event.mCancelled)
                            source.mDamage = (int32_t) event.mAmount;
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ITEM_BREAK), after.mItemBreak,
                    [&plugins](ItemBreakAfterEvent &source) {
                        ItemStack broken = source.mItem;
                        PluginEvent event;
                        event.mType = FALCON_EVENT_ITEM_BREAK;
                        event.mPlayer = source.mPlayer;
                        event.mItem = &broken;
                        plugins.dispatch(event);
                    });
}
