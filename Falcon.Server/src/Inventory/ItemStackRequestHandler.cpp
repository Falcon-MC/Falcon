#include "Inventory/ItemStackRequestHandler.h"

#include "Core/Debug/BedrockLog.h"
#include "Inventory/BundleInventory.h"
#include "Item/EnchantmentData.h"
#include "Item/EnchantmentHelper.h"
#include "Item/Components/ItemComponentTypes.h"
#include "Item/ItemData.h"
#include "Item/ItemEnchantments.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace {

    struct TouchedSlot {
        FullContainerName mContainerName;
        int mSlot = 0;
    };

    struct BundleView {
        int32_t mId = 0;
        ContainerSlotType mOwnerContainer = ContainerSlotType::Unknown;
        int mOwnerSlot = 0;
        std::vector<ItemStack> mContents;
    };

    struct RequestContext {
        const std::vector<CreativeItemData> *mCreativeItems = nullptr;
        const std::vector<ItemStack> *mRecipeOutputs = nullptr;
        const std::vector<uint32_t> *mRecipeSourceIndices = nullptr;
        ItemStack mCreatedOutput;
        std::vector<ItemStack> *mDroppedItems = nullptr;
        bool mCraftingTableOpen = false;
        bool mFurnaceOpen = false;
        Container *mOpenContainer = nullptr;
        std::vector<ItemStack> mContainerSlots;
        CraftingRecipeMatch mCraftingMatch;
        std::vector<int32_t> mCraftingRequired;
        std::vector<int32_t> mCraftingConsumed;
        int mCraftingGridWidth = 0;
        bool mCraftingActive = false;
        bool mCraftRecipeSeen = false;
        bool mCreatedOutputActive = false;
        bool mCreatedOutputCreative = false;
        const PacketCodecContext *mCodecContext = nullptr;
        std::vector<std::unique_ptr<BundleView>> mBundles;
        int32_t mPlayerXpLevel = 0;
        bool mCreativeMode = false;
        int32_t mEnchantLevelsConsumed = 0;
        bool mEnchantActive = false;
        int32_t mEnchantMaterialRequired = 0;
        int32_t mEnchantInputConsumed = 0;
        int32_t mEnchantMaterialConsumed = 0;
    };

    ItemStack *findBundleOwner(PlayerInventory &inventory, RequestContext &context, int32_t bundleId,
                               ContainerSlotType &outContainer, int &outSlot) {
        if (context.mOpenContainer != nullptr) {
            for (size_t slot = 0; slot < context.mContainerSlots.size(); ++slot) {
                ItemStack &item = context.mContainerSlots[slot];
                if (!BundleInventory::isBundle(item) || BundleInventory::getBundleId(item) != bundleId) {
                    continue;
                }
                outContainer = ContainerSlotType::LevelEntity;
                outSlot = (int) slot;
                return &item;
            }
        }

        for (int slot = 0; slot < PlayerInventory::CONTAINER_SIZE; ++slot) {
            ItemStack *item = inventory.resolveSlot(ContainerSlotType::Inventory, slot);
            if (item == nullptr || !BundleInventory::isBundle(*item)) {
                continue;
            }
            if (BundleInventory::getBundleId(*item) != bundleId) {
                continue;
            }
            outContainer = ContainerSlotType::Inventory;
            outSlot = slot;
            return item;
        }

        ItemStack *cursor = inventory.resolveSlot(ContainerSlotType::Cursor, 0);
        if (cursor != nullptr && BundleInventory::isBundle(*cursor)
            && BundleInventory::getBundleId(*cursor) == bundleId) {
            outContainer = ContainerSlotType::Cursor;
            outSlot = 0;
            return cursor;
        }

        ItemStack *offhand = inventory.resolveSlot(ContainerSlotType::Offhand, 1);
        if (offhand != nullptr && BundleInventory::isBundle(*offhand)
            && BundleInventory::getBundleId(*offhand) == bundleId) {
            outContainer = ContainerSlotType::Offhand;
            outSlot = 1;
            return offhand;
        }

        return nullptr;
    }

    BundleView *resolveBundle(PlayerInventory &inventory, RequestContext &context, int32_t bundleId) {
        if (context.mCodecContext == nullptr) {
            return nullptr;
        }

        for (const std::unique_ptr<BundleView> &view: context.mBundles) {
            if (view->mId == bundleId) {
                return view.get();
            }
        }

        ContainerSlotType ownerContainer = ContainerSlotType::Unknown;
        int ownerSlot = 0;
        ItemStack *owner = findBundleOwner(inventory, context, bundleId, ownerContainer, ownerSlot);
        if (owner == nullptr) {
            return nullptr;
        }

        std::unique_ptr<BundleView> view(new BundleView());
        view->mId = bundleId;
        view->mOwnerContainer = ownerContainer;
        view->mOwnerSlot = ownerSlot;
        view->mContents = BundleInventory::readContents(*owner, *context.mCodecContext);
        context.mBundles.push_back(std::move(view));
        return context.mBundles.back().get();
    }

    ItemStack *resolveSlot(PlayerInventory &inventory, RequestContext &context, const FullContainerName &name,
                           int slot) {
        const ContainerSlotType container = name.mContainer;
        if (container == ContainerSlotType::DynamicContainer) {
            if (!name.mHasDynamicId || slot < 0 || slot >= BundleInventory::SIZE) {
                return nullptr;
            }
            BundleView *view = resolveBundle(inventory, context, name.mDynamicId);
            if (view == nullptr) {
                return nullptr;
            }
            return &view->mContents[(size_t) slot];
        }
        if (container == ContainerSlotType::CreatedOutput) {
            return (slot == 0 || slot == 50) ? &context.mCreatedOutput : nullptr;
        }
        if (container == ContainerSlotType::LevelEntity || container == ContainerSlotType::Barrel
            || container == ContainerSlotType::ShulkerBox
            || container == ContainerSlotType::CrafterBlockContainer) {
            if (context.mOpenContainer == nullptr || slot < 0
                || (size_t) slot >= context.mContainerSlots.size()) {
                return nullptr;
            }
            return &context.mContainerSlots[(size_t) slot];
        }
        if (container == ContainerSlotType::CraftingInput
            && slot >= PlayerInventory::CRAFTING_TABLE_NETWORK_SLOT_FIRST
            && slot < PlayerInventory::CRAFTING_TABLE_NETWORK_SLOT_FIRST + PlayerInventory::CRAFTING_TABLE_SIZE
            && !context.mCraftingTableOpen) {
            return nullptr;
        }
        if (container == ContainerSlotType::FurnaceIngredient || container == ContainerSlotType::BlastFurnaceIngredient
            || container == ContainerSlotType::SmokerIngredient || container == ContainerSlotType::FurnaceFuel
            || container == ContainerSlotType::FurnaceResult) {
            if (!context.mFurnaceOpen) {
                return nullptr;
            }
            return inventory.resolveSlot(container, slot);
        }
        return inventory.resolveSlot(container, slot);
    }

    int craftingSlotIndex(ContainerSlotType container, int slot, int gridWidth) {
        if (container != ContainerSlotType::CraftingInput) {
            return -1;
        }
        if (gridWidth == 2 && slot >= PlayerInventory::CRAFTING_NETWORK_SLOT_FIRST
            && slot < PlayerInventory::CRAFTING_NETWORK_SLOT_FIRST + PlayerInventory::CRAFTING_SIZE) {
            return slot - PlayerInventory::CRAFTING_NETWORK_SLOT_FIRST;
        }
        if (gridWidth == 3 && slot >= PlayerInventory::CRAFTING_TABLE_NETWORK_SLOT_FIRST
            && slot < PlayerInventory::CRAFTING_TABLE_NETWORK_SLOT_FIRST + PlayerInventory::CRAFTING_TABLE_SIZE) {
            return slot - PlayerInventory::CRAFTING_TABLE_NETWORK_SLOT_FIRST;
        }
        return -1;
    }

    bool sameContainerName(const FullContainerName &left, const FullContainerName &right) {
        if (left.mContainer != right.mContainer) {
            return false;
        }
        if (left.mContainer != ContainerSlotType::DynamicContainer) {
            return true;
        }
        return left.mHasDynamicId == right.mHasDynamicId && left.mDynamicId == right.mDynamicId;
    }

    void markTouched(std::vector<TouchedSlot> &touched, const FullContainerName &name, int slot) {
        for (const TouchedSlot &entry: touched) {
            if (sameContainerName(entry.mContainerName, name) && entry.mSlot == slot) {
                return;
            }
        }

        TouchedSlot entry;
        entry.mContainerName = name;
        entry.mSlot = slot;
        touched.push_back(entry);
    }

    ItemStackResponseSlot describeSlot(int slot, const ItemStack &item) {
        ItemStackResponseSlot response;
        response.mSlot = slot;
        response.mHotbarSlot = slot;
        response.mCount = item.isAir() ? 0 : item.mCount;
        response.mStackNetworkId = item.mUsingNetId ? item.mNetId : 0;
        response.mDurabilityCorrection = item.isAir() ? 0 : item.mDamage;
        return response;
    }

    void appendResponseSlot(std::vector<ItemStackResponseContainer> &containers, const FullContainerName &container,
                            int slot, const ItemStack &item) {
        for (ItemStackResponseContainer &existing: containers) {
            if (!sameContainerName(existing.mContainerName, container)) {
                continue;
            }

            for (ItemStackResponseSlot &existingSlot: existing.mItems) {
                if (existingSlot.mSlot == slot) {
                    existingSlot = describeSlot(slot, item);
                    return;
                }
            }

            existing.mItems.push_back(describeSlot(slot, item));
            return;
        }

        ItemStackResponseContainer created;
        created.mContainerName = container;
        created.mItems.push_back(describeSlot(slot, item));
        containers.push_back(created);
    }

    bool acceptsBundleWeight(RequestContext &context, const std::vector<ItemStack> &projected) {
        return BundleInventory::getTotalWeight(projected, *context.mCodecContext) <= BundleInventory::MAX_FILL;
    }

    bool checkBundleCapacity(PlayerInventory &inventory, RequestContext &context,
                             const FullContainerName &destination, int slot, const ItemStack &source, int count) {
        if (destination.mContainer != ContainerSlotType::DynamicContainer) {
            return true;
        }
        if (context.mCodecContext == nullptr || slot < 0 || slot >= BundleInventory::SIZE) {
            return false;
        }

        BundleView *view = resolveBundle(inventory, context, destination.mDynamicId);
        if (view == nullptr) {
            return false;
        }

        if (BundleInventory::isBundle(source) && BundleInventory::getBundleId(source) == destination.mDynamicId) {
            return false;
        }

        std::vector<ItemStack> projected = view->mContents;
        ItemStack &target = projected[(size_t) slot];
        if (target.isAir()) {
            target = source;
            target.mCount = count;
        } else {
            target.mCount += count;
        }

        return acceptsBundleWeight(context, projected);
    }

    bool checkBundleSwap(PlayerInventory &inventory, RequestContext &context, const FullContainerName &destination,
                         int slot, const ItemStack &incoming) {
        if (destination.mContainer != ContainerSlotType::DynamicContainer) {
            return true;
        }
        if (context.mCodecContext == nullptr || slot < 0 || slot >= BundleInventory::SIZE) {
            return false;
        }

        BundleView *view = resolveBundle(inventory, context, destination.mDynamicId);
        if (view == nullptr) {
            return false;
        }

        if (BundleInventory::isBundle(incoming) && BundleInventory::getBundleId(incoming) == destination.mDynamicId) {
            return false;
        }

        std::vector<ItemStack> projected = view->mContents;
        projected[(size_t) slot] = incoming;
        return acceptsBundleWeight(context, projected);
    }

    bool moveItems(PlayerInventory &inventory, RequestContext &context, const ItemStackRequestAction &action,
                   std::vector<TouchedSlot> &touched) {
        ItemStack *source = resolveSlot(inventory, context, action.mSource.mContainerName,
                                        action.mSource.mSlot);
        ItemStack *destination = resolveSlot(inventory, context, action.mDestination.mContainerName,
                                             action.mDestination.mSlot);

        if (source == nullptr || destination == nullptr || source == destination) {
            return false;
        }

        if (action.mDestination.mContainerName.mContainer == ContainerSlotType::FurnaceResult) {
            return false;
        }

        const bool creativeSource = context.mCreatedOutputCreative && source == &context.mCreatedOutput;

        if (source->isAir() || action.mCount <= 0 || (!creativeSource && action.mCount > source->mCount)) {
            return false;
        }

        if (!destination->isAir() && !PlayerInventory::canStack(*source, *destination)) {
            return false;
        }

        if (!destination->isAir()
            && destination->mCount + action.mCount > PlayerInventory::getMaxStackSize(*destination)) {
            return false;
        }

        if (!checkBundleCapacity(inventory, context, action.mDestination.mContainerName, action.mDestination.mSlot,
                                 *source, action.mCount)) {
            return false;
        }

        if (destination->isAir()) {
            ItemStack moved = *source;
            moved.mCount = action.mCount;
            *destination = moved;
        } else {
            destination->mCount += action.mCount;
        }

        if (creativeSource) {
            *source = ItemStack::air();
        } else {
            source->mCount -= action.mCount;
            if (source->mCount <= 0) {
                *source = ItemStack::air();
            }
        }

        inventory.assignNetId(*source);
        inventory.assignNetId(*destination);

        markTouched(touched, action.mSource.mContainerName, action.mSource.mSlot);
        markTouched(touched, action.mDestination.mContainerName, action.mDestination.mSlot);
        return true;
    }

    bool swapItems(PlayerInventory &inventory, RequestContext &context, const ItemStackRequestAction &action,
                   std::vector<TouchedSlot> &touched) {
        ItemStack *source = resolveSlot(inventory, context, action.mSource.mContainerName,
                                        action.mSource.mSlot);
        ItemStack *destination = resolveSlot(inventory, context, action.mDestination.mContainerName,
                                             action.mDestination.mSlot);

        if (source == nullptr || destination == nullptr || source == destination) {
            return false;
        }

        if (action.mSource.mContainerName.mContainer == ContainerSlotType::FurnaceResult
            || action.mDestination.mContainerName.mContainer == ContainerSlotType::FurnaceResult) {
            return false;
        }

        if (!checkBundleSwap(inventory, context, action.mSource.mContainerName, action.mSource.mSlot, *destination)
            || !checkBundleSwap(inventory, context, action.mDestination.mContainerName, action.mDestination.mSlot,
                                *source)) {
            return false;
        }

        std::swap(*source, *destination);

        inventory.assignNetId(*source);
        inventory.assignNetId(*destination);

        markTouched(touched, action.mSource.mContainerName, action.mSource.mSlot);
        markTouched(touched, action.mDestination.mContainerName, action.mDestination.mSlot);
        return true;
    }

    bool removeItems(PlayerInventory &inventory, RequestContext &context, const ItemStackRequestAction &action,
                     std::vector<TouchedSlot> &touched, bool dropped = false) {
        ItemStack *source = resolveSlot(inventory, context, action.mSource.mContainerName,
                                        action.mSource.mSlot);
        if (source == nullptr || source->isAir() || action.mCount <= 0 || action.mCount > source->mCount) {
            return false;
        }

        if (action.mSource.mContainerName.mContainer == ContainerSlotType::FurnaceResult) {
            return false;
        }

        if (context.mCraftingActive && action.mType == ItemStackRequestActionType::Consume) {
            const int gridSlot = craftingSlotIndex(action.mSource.mContainerName.mContainer,
                                                   action.mSource.mSlot, context.mCraftingGridWidth);
            if (gridSlot < 0 || (size_t) gridSlot >= context.mCraftingRequired.size()
                || context.mCraftingConsumed[(size_t) gridSlot] + action.mCount
                   > context.mCraftingRequired[(size_t) gridSlot]) {
                return false;
            }
            context.mCraftingConsumed[(size_t) gridSlot] += action.mCount;
        }

        if (context.mEnchantActive && action.mType == ItemStackRequestActionType::Consume) {
            const ContainerSlotType container = action.mSource.mContainerName.mContainer;
            if (container == ContainerSlotType::EnchantingInput)
                context.mEnchantInputConsumed += action.mCount;
            else if (container == ContainerSlotType::EnchantingMaterial)
                context.mEnchantMaterialConsumed += action.mCount;
        }

        if (dropped && context.mDroppedItems != nullptr) {
            ItemStack drop = *source;
            drop.mCount = action.mCount;
            drop.mUsingNetId = false;
            drop.mNetId = 0;
            context.mDroppedItems->push_back(drop);
        }

        source->mCount -= action.mCount;
        if (source->mCount <= 0) {
            *source = ItemStack::air();
        }

        inventory.assignNetId(*source);
        markTouched(touched, action.mSource.mContainerName, action.mSource.mSlot);
        return true;
    }

    bool createCreativeItem(RequestContext &context, const ItemStackRequestAction &action) {
        if (context.mCreativeItems == nullptr) {
            return false;
        }

        for (const CreativeItemData &entry: *context.mCreativeItems) {
            if (entry.mNetId != action.mCreativeItemNetworkId) {
                continue;
            }

            context.mCreatedOutput = entry.mItem;
            context.mCreatedOutputActive = true;
            context.mCreatedOutputCreative = true;
            return true;
        }

        return false;
    }

    bool craftEnchant(PlayerInventory &inventory, RequestContext &context, const ItemStackRequestAction &action,
                      std::vector<TouchedSlot> &touched) {
        std::vector<EnchantmentInstance> enchantments;
        int32_t consumeCost = 0;
        int32_t requiredLevel = 0;
        if (!EnchantmentHelper::takeOption(action.mRecipeNetworkId, enchantments, consumeCost, requiredLevel))
            return false;

        ItemStack *input = inventory.resolveSlot(ContainerSlotType::EnchantingInput, 0);
        if (input == nullptr || input->isAir())
            return false;

        ItemStack *lapis = inventory.resolveSlot(ContainerSlotType::EnchantingMaterial, 0);

        if (!context.mCreativeMode) {
            if (context.mPlayerXpLevel < requiredLevel)
                return false;

            if (lapis == nullptr || lapis->isAir() || lapis->mCount < consumeCost)
                return false;
        }

        ItemStack enchanted = *input;
        enchanted.mCount = 1;
        if (context.mCodecContext != nullptr && enchanted.mDefinition != nullptr
            && enchanted.mDefinition->getIdentifier() == "minecraft:book") {
            std::shared_ptr<ItemDefinition> enchantedBook =
                    context.mCodecContext->getItemDefinitions().getDefinition("minecraft:enchanted_book");
            if (enchantedBook != nullptr)
                enchanted.mDefinition = std::move(enchantedBook);
        }
        ItemEnchantments::write(enchanted, enchantments);

        context.mCreatedOutput = std::move(enchanted);
        context.mCreatedOutputActive = true;
        context.mEnchantActive = true;
        context.mEnchantMaterialRequired = context.mCreativeMode ? 0 : consumeCost;

        if (!context.mCreativeMode)
            context.mEnchantLevelsConsumed = consumeCost;

        return true;
    }

    bool craftRecipe(PlayerInventory &inventory, RequestContext &context, const ItemStackRequestAction &action,
                     std::vector<TouchedSlot> &touched) {
        if (context.mCraftRecipeSeen) {
            return false;
        }
        context.mCraftRecipeSeen = true;

        if (action.mRecipeNetworkId >= EnchantmentHelper::RECIPE_ID_BASE) {
            return craftEnchant(inventory, context, action, touched);
        }

        if (context.mRecipeOutputs == nullptr) {
            return false;
        }

        const int32_t netId = action.mRecipeNetworkId;
        if (context.mRecipeSourceIndices == nullptr || netId <= 0
            || (size_t) netId > context.mRecipeSourceIndices->size()) {
            return false;
        }

        const ItemStack &output = (*context.mRecipeOutputs)[(size_t) netId - 1];
        if (output.isAir()) {
            return false;
        }

        const int repetitions = action.mNumberOfRequestedCrafts <= 0 ? 1 : action.mNumberOfRequestedCrafts;
        if (repetitions > 256) {
            return false;
        }

        bool tableHasItems = false;
        for (const ItemStack &item: inventory.getCraftingTableContents()) {
            if (!item.isAir()) {
                tableHasItems = true;
                break;
            }
        }

        if (tableHasItems && !context.mCraftingTableOpen) {
            return false;
        }

        const int gridWidth = tableHasItems ? 3 : 2;
        const std::vector<ItemStack> &grid = tableHasItems
                                             ? inventory.getCraftingTableContents()
                                             : inventory.getCraftingContents();
        CraftingRecipeMatch match;
        if (!CraftingManager::matchNetworkRecipe(grid, gridWidth, netId, *context.mRecipeOutputs,
                                                 *context.mRecipeSourceIndices, match)) {
            return false;
        }

        std::vector<int32_t> required = match.mRequiredBySlot;
        for (int32_t &count: required) {
            count *= repetitions;
        }
        for (size_t slot = 0; slot < required.size(); ++slot) {
            if (required[slot] > 0 && grid[slot].mCount < required[slot]) {
                return false;
            }
        }

        context.mCreatedOutput = output;
        context.mCreatedOutput.mCount = output.mCount * repetitions;
        context.mCreatedOutputActive = true;
        context.mCraftingMatch = std::move(match);
        context.mCraftingRequired = std::move(required);
        context.mCraftingConsumed.assign(context.mCraftingRequired.size(), 0);
        context.mCraftingGridWidth = gridWidth;
        context.mCraftingActive = true;
        return true;
    }

    bool isBoundArmor(PlayerInventory &inventory, RequestContext &context, const FullContainerName &name, int slot) {
        if (context.mCreativeMode || name.mContainer != ContainerSlotType::Armor)
            return false;

        const ItemStack *item = resolveSlot(inventory, context, name, slot);
        return item != nullptr && ItemEnchantments::getLevel(*item, EnchantmentIds::BINDING) > 0;
    }

    bool rejectedByOffhand(PlayerInventory &inventory, RequestContext &context, const FullContainerName &destination,
                           const FullContainerName &source, int sourceSlot) {
        if (destination.mContainer != ContainerSlotType::Offhand)
            return false;

        const ItemStack *item = resolveSlot(inventory, context, source, sourceSlot);
        if (item == nullptr || item->isAir() || item->mDefinition == nullptr)
            return false;

        const std::string &identifier = item->mDefinition->getIdentifier();
        if (ItemDataTable::find(identifier) == nullptr)
            return false;

        return ItemDataTable::getComponents(identifier).get<AllowOffHandItemComponent>() == nullptr;
    }

    bool applyAction(PlayerInventory &inventory, RequestContext &context, const ItemStackRequestAction &action,
                     std::vector<TouchedSlot> &touched) {
        switch (action.mType) {
            case ItemStackRequestActionType::Take:
            case ItemStackRequestActionType::Place:
                if (rejectedByOffhand(inventory, context, action.mDestination.mContainerName,
                                      action.mSource.mContainerName, action.mSource.mSlot))
                    return false;
                break;

            case ItemStackRequestActionType::Swap:
                if (rejectedByOffhand(inventory, context, action.mDestination.mContainerName,
                                      action.mSource.mContainerName, action.mSource.mSlot)
                    || rejectedByOffhand(inventory, context, action.mSource.mContainerName,
                                         action.mDestination.mContainerName, action.mDestination.mSlot))
                    return false;
                break;

            default:
                break;
        }

        switch (action.mType) {
            case ItemStackRequestActionType::Take:
            case ItemStackRequestActionType::Place:
            case ItemStackRequestActionType::Drop:
            case ItemStackRequestActionType::Destroy:
                if (isBoundArmor(inventory, context, action.mSource.mContainerName, action.mSource.mSlot))
                    return false;
                break;

            case ItemStackRequestActionType::Swap:
                if (isBoundArmor(inventory, context, action.mSource.mContainerName, action.mSource.mSlot)
                    || isBoundArmor(inventory, context, action.mDestination.mContainerName, action.mDestination.mSlot))
                    return false;
                break;

            default:
                break;
        }

        switch (action.mType) {
            case ItemStackRequestActionType::Take:
            case ItemStackRequestActionType::Place:
                return moveItems(inventory, context, action, touched);

            case ItemStackRequestActionType::Swap:
                return swapItems(inventory, context, action, touched);

            case ItemStackRequestActionType::Drop:
                return removeItems(inventory, context, action, touched, true);

            case ItemStackRequestActionType::Destroy:
            case ItemStackRequestActionType::Consume:
                return removeItems(inventory, context, action, touched);

            case ItemStackRequestActionType::CraftCreative:
                return createCreativeItem(context, action);

            case ItemStackRequestActionType::CraftRecipe:
            case ItemStackRequestActionType::CraftRecipeAuto:
                return craftRecipe(inventory, context, action, touched);

            case ItemStackRequestActionType::Create:
            case ItemStackRequestActionType::CraftResultsDeprecated:
            case ItemStackRequestActionType::CraftNonImplemented:
            case ItemStackRequestActionType::MineBlock:
            case ItemStackRequestActionType::LabTableCombine:
            case ItemStackRequestActionType::BeaconPayment:
                return true;

            default:
                LOG_WARN(LogAreaID::Server, "Unhandled item stack request action %d",
                         itemStackRequestActionTypeToId(action.mType));
                return false;
        }
    }

}

ItemStackResponseEntry ItemStackRequestHandler::execute(PlayerInventory &inventory, const ItemStackRequest &request,
                                                       const std::vector<CreativeItemData> &creativeItems,
                                                       const std::vector<ItemStack> &recipeOutputs,
                                                       const std::vector<uint32_t> &recipeSourceIndices,
                                                       bool craftingTableOpen,
                                                       bool furnaceOpen,
                                                       Container *openContainer,
                                                       std::vector<ItemStack> *outDroppedItems,
                                                       const PacketCodecContext *codecContext,
                                                       std::vector<BundleSyncData> *outBundles,
                                                       int32_t playerXpLevel,
                                                       bool creativeMode,
                                                       int32_t *outEnchantLevelsConsumed) {
    ItemStackResponseEntry entry;
    entry.mRequestId = request.mRequestId;
    entry.mResult = RESULT_OK;

    PlayerInventory working = inventory;
    std::vector<TouchedSlot> touched;
    std::vector<ItemStack> localDroppedItems;

    RequestContext context;
    context.mCreativeItems = &creativeItems;
    context.mRecipeOutputs = &recipeOutputs;
    context.mRecipeSourceIndices = &recipeSourceIndices;
    context.mCreatedOutput = ItemStack::air();
    context.mDroppedItems = &localDroppedItems;
    context.mCraftingTableOpen = craftingTableOpen;
    context.mFurnaceOpen = furnaceOpen;
    context.mOpenContainer = openContainer;
    context.mCodecContext = codecContext;
    context.mPlayerXpLevel = playerXpLevel;
    context.mCreativeMode = creativeMode;

    if (openContainer != nullptr) {
        const int containerSize = openContainer->getContainerSize();
        context.mContainerSlots.reserve((size_t) containerSize);
        for (int slot = 0; slot < containerSize; ++slot)
            context.mContainerSlots.push_back(openContainer->getContainerItem(slot));
    }

    for (const ItemStackRequestAction &action: request.mActions) {
        if (!applyAction(working, context, action, touched)) {
            entry.mResult = RESULT_ERROR;
            entry.mContainers.clear();
            return entry;
        }
    }

    if (context.mCraftingActive) {
        if (context.mCraftingRequired.size() != context.mCraftingConsumed.size()) {
            entry.mResult = RESULT_ERROR;
            return entry;
        }
        for (size_t slot = 0; slot < context.mCraftingRequired.size(); ++slot) {
            if (context.mCraftingRequired[slot] != context.mCraftingConsumed[slot]) {
                entry.mResult = RESULT_ERROR;
                entry.mContainers.clear();
                return entry;
            }
        }
    }

    if (context.mEnchantActive
        && (context.mEnchantInputConsumed != 1
            || context.mEnchantMaterialConsumed < context.mEnchantMaterialRequired)) {
        entry.mResult = RESULT_ERROR;
        entry.mContainers.clear();
        return entry;
    }

    if (context.mCreatedOutputActive && !context.mCreatedOutput.isAir()) {
        entry.mResult = RESULT_ERROR;
        entry.mContainers.clear();
        return entry;
    }

    for (const std::unique_ptr<BundleView> &view: context.mBundles) {
        FullContainerName ownerName;
        ownerName.mContainer = view->mOwnerContainer;

        ItemStack *owner = resolveSlot(working, context, ownerName, view->mOwnerSlot);
        if (owner == nullptr || !BundleInventory::isBundle(*owner)
            || BundleInventory::getBundleId(*owner) != view->mId) {
            continue;
        }

        BundleInventory::writeContents(*owner, view->mContents);
        markTouched(touched, ownerName, view->mOwnerSlot);

        if (outBundles != nullptr) {
            BundleSyncData data;
            data.mBundleId = view->mId;
            data.mOwnerContainer = view->mOwnerContainer;
            data.mOwnerSlot = view->mOwnerSlot;
            data.mContents = view->mContents;
            outBundles->push_back(std::move(data));
        }
    }

    if (openContainer != nullptr) {
        for (size_t slot = 0; slot < context.mContainerSlots.size(); ++slot)
            openContainer->setContainerItem((int) slot, context.mContainerSlots[slot]);
    }

    inventory = std::move(working);
    if (outDroppedItems != nullptr) {
        outDroppedItems->insert(outDroppedItems->end(), localDroppedItems.begin(), localDroppedItems.end());
    }

    if (outEnchantLevelsConsumed != nullptr)
        *outEnchantLevelsConsumed = context.mEnchantLevelsConsumed;

    for (const TouchedSlot &slot: touched) {
        if (slot.mContainerName.mContainer == ContainerSlotType::CreatedOutput) {
            continue;
        }

        if (slot.mContainerName.mContainer == ContainerSlotType::DynamicContainer) {
            const BundleView *view = nullptr;
            for (const std::unique_ptr<BundleView> &candidate: context.mBundles) {
                if (candidate->mId == slot.mContainerName.mDynamicId) {
                    view = candidate.get();
                    break;
                }
            }

            if (view == nullptr || slot.mSlot < 0 || slot.mSlot >= BundleInventory::SIZE) {
                continue;
            }

            appendResponseSlot(entry.mContainers, slot.mContainerName, slot.mSlot,
                               view->mContents[(size_t) slot.mSlot]);
            continue;
        }

        if (slot.mContainerName.mContainer == ContainerSlotType::LevelEntity
            || slot.mContainerName.mContainer == ContainerSlotType::Barrel
            || slot.mContainerName.mContainer == ContainerSlotType::ShulkerBox
            || slot.mContainerName.mContainer == ContainerSlotType::CrafterBlockContainer) {
            if (openContainer == nullptr || slot.mSlot < 0
                || (size_t) slot.mSlot >= context.mContainerSlots.size()) {
                continue;
            }

            appendResponseSlot(entry.mContainers, slot.mContainerName, slot.mSlot,
                               context.mContainerSlots[(size_t) slot.mSlot]);
            continue;
        }

        const ItemStack *item = inventory.resolveSlot(slot.mContainerName.mContainer, slot.mSlot);
        if (item == nullptr) {
            continue;
        }

        appendResponseSlot(entry.mContainers, slot.mContainerName, slot.mSlot, *item);
    }

    return entry;
}
