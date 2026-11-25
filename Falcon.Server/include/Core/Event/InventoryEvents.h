#pragma once

#include "Core/Event/Event.h"
#include "Core/Event/EventSignal.h"
#include "Core/Math/Vector3i.h"
#include "Protocol/Packets/BookEditPacket.h"
#include "Protocol/Types/EnchantOptionData.h"
#include "Protocol/Types/ItemStack.h"

#include <cstdint>
#include <utility>
#include <vector>

class Actor;
class ItemActor;
class ItemStackRequest;
class Level;
class ServerPlayer;

// a furnace about to light with its fuel item; cancelling, or no burn time left, keeps it unlit
struct FurnaceBurnBeforeEvent : public CancellableEvent {
    Level &mLevel;
    Vector3i mBlockPosition;
    const ItemStack &mFuel;
    // ticks the fuel burns
    int32_t mBurnTime;

    FurnaceBurnBeforeEvent(Level &level, const Vector3i &position, const ItemStack &fuel, int32_t burnTime)
            : mLevel(level), mBlockPosition(position), mFuel(fuel), mBurnTime(burnTime) {}
};

// a furnace about to turn its input into the result; the result can change
struct FurnaceSmeltBeforeEvent : public CancellableEvent {
    Level &mLevel;
    Vector3i mBlockPosition;
    const ItemStack &mInput;
    ItemStack &mResult;

    FurnaceSmeltBeforeEvent(Level &level, const Vector3i &position, const ItemStack &input, ItemStack &result)
            : mLevel(level), mBlockPosition(position), mInput(input), mResult(result) {}
};

// a player about to open the container of a block
struct BlockContainerOpenBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    Level &mLevel;
    Vector3i mBlockPosition;

    BlockContainerOpenBeforeEvent(ServerPlayer &player, Level &level, const Vector3i &position)
            : mPlayer(player), mLevel(level), mBlockPosition(position) {}
};

// a block container opened through BlockContainerOpenBeforeEvent, now closed
struct BlockContainerCloseAfterEvent : public Event {
    ServerPlayer &mPlayer;
    Level &mLevel;
    Vector3i mBlockPosition;

    BlockContainerCloseAfterEvent(ServerPlayer &player, Level &level, const Vector3i &position)
            : mPlayer(player), mLevel(level), mBlockPosition(position) {}
};

// an item stack request of a player before any of its actions run; cancelling rejects the whole request
struct InventoryRequestBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    const ItemStackRequest &mRequest;

    InventoryRequestBeforeEvent(ServerPlayer &player, const ItemStackRequest &request)
            : mPlayer(player), mRequest(request) {}
};

// a player about to select another hotbar slot
struct PlayerItemHeldBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    int32_t mPreviousSlot;
    int32_t mSlot;
    const ItemStack &mItem;

    PlayerItemHeldBeforeEvent(ServerPlayer &player, int32_t previousSlot, int32_t slot, const ItemStack &item)
            : mPlayer(player), mPreviousSlot(previousSlot), mSlot(slot), mItem(item) {}
};

// a player about to throw an item out of their inventory
struct PlayerDropItemBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    const ItemStack &mItem;

    PlayerDropItemBeforeEvent(ServerPlayer &player, const ItemStack &item) : mPlayer(player), mItem(item) {}
};

// the options an enchanting table is about to offer; cancelling offers none
struct EnchantingOptionsBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    Level &mLevel;
    Vector3i mBlockPosition;
    const ItemStack &mInput;
    const ItemStack &mMaterial;
    const std::vector<EnchantOptionData> &mOptions;
    // replaces mOptions when set: the table discards the options it rolled first
    std::vector<EnchantOptionData> mReplacement;
    bool mReplaced = false;

    EnchantingOptionsBeforeEvent(ServerPlayer &player, Level &level, const Vector3i &position, const ItemStack &input,
                                 const ItemStack &material, const std::vector<EnchantOptionData> &options)
            : mPlayer(player), mLevel(level), mBlockPosition(position), mInput(input), mMaterial(material),
              mOptions(options) {}
};

// an anvil just used by a player
struct AnvilUseAfterEvent : public Event {
    ServerPlayer &mPlayer;
    Level &mLevel;
    Vector3i mBlockPosition;
    int32_t mLevelsConsumed;

    AnvilUseAfterEvent(ServerPlayer &player, Level &level, const Vector3i &position, int32_t levelsConsumed)
            : mPlayer(player), mLevel(level), mBlockPosition(position), mLevelsConsumed(levelsConsumed) {}
};

// the armor of a player may have changed; the four slots as they were before
struct PlayerArmorChangeAfterEvent : public Event {
    ServerPlayer &mPlayer;
    const ItemStack (&mPrevious)[4];

    PlayerArmorChangeAfterEvent(ServerPlayer &player, const ItemStack (&previous)[4])
            : mPlayer(player), mPrevious(previous) {}
};

// a player about to interact with an actor
struct PlayerInteractEntityBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    Actor &mTarget;

    PlayerInteractEntityBeforeEvent(ServerPlayer &player, Actor &target) : mPlayer(player), mTarget(target) {}
};

// a player about to pick up part or all of an item on the ground
struct PlayerPickupItemBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    ItemActor &mItemActor;
    // what the player takes, the count limited by the room in their inventory
    const ItemStack &mItem;

    PlayerPickupItemBeforeEvent(ServerPlayer &player, ItemActor &itemActor, const ItemStack &item)
            : mPlayer(player), mItemActor(itemActor), mItem(item) {}
};

// an item on the ground about to merge into another
struct ItemMergeBeforeEvent : public CancellableEvent {
    ItemActor &mMerged;
    ItemActor &mTarget;

    ItemMergeBeforeEvent(ItemActor &merged, ItemActor &target) : mMerged(merged), mTarget(target) {}
};

// an item on the ground about to despawn of age; cancelling restarts its age
struct ItemDespawnBeforeEvent : public CancellableEvent {
    ItemActor &mItemActor;

    explicit ItemDespawnBeforeEvent(ItemActor &itemActor) : mItemActor(itemActor) {}
};

// a player about to use the item in hand
struct PlayerItemUseBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    const ItemStack &mItem;

    PlayerItemUseBeforeEvent(ServerPlayer &player, const ItemStack &item) : mPlayer(player), mItem(item) {}
};

// a player about to finish eating or drinking an item
struct PlayerItemConsumeBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    const ItemStack &mItem;

    PlayerItemConsumeBeforeEvent(ServerPlayer &player, const ItemStack &item) : mPlayer(player), mItem(item) {}
};

// a player about to edit or sign a book; the edited book can change, it must stay a book
struct PlayerEditBookBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    const ItemStack &mBook;
    ItemStack &mResult;
    BookEditPacket::Action mAction;
    int32_t mPage;
    int32_t mSecondaryPage;

    PlayerEditBookBeforeEvent(ServerPlayer &player, const ItemStack &book, ItemStack &result,
                              BookEditPacket::Action action, int32_t page, int32_t secondaryPage)
            : mPlayer(player), mBook(book), mResult(result), mAction(action), mPage(page),
              mSecondaryPage(secondaryPage) {}
};

// a crafting request that passed every check, before its result is taken
struct CraftItemBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    // the recipe output times the repetitions
    const ItemStack &mOutput;
    int32_t mRepetitions;
    int32_t mRecipeId;
    std::vector<ItemStack> &mInputs;
    int32_t mGridWidth;
    // 0 for a recipe without a shape
    int32_t mRecipeWidth;
    // grid slot and count each repetition takes
    const std::vector<std::pair<int32_t, int32_t>> &mSlots;

    CraftItemBeforeEvent(ServerPlayer &player, const ItemStack &output, int32_t repetitions, int32_t recipeId,
                         std::vector<ItemStack> &inputs, int32_t gridWidth, int32_t recipeWidth,
                         const std::vector<std::pair<int32_t, int32_t>> &slots)
            : mPlayer(player), mOutput(output), mRepetitions(repetitions), mRecipeId(recipeId), mInputs(inputs),
              mGridWidth(gridWidth), mRecipeWidth(recipeWidth), mSlots(slots) {}
};

// an enchanting table request that passed every check, before the item is enchanted
struct ItemEnchantBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    ItemStack &mInput;
    ItemStack &mResult;
    int32_t mCost;
    int32_t mRequiredLevel;

    ItemEnchantBeforeEvent(ServerPlayer &player, ItemStack &input, ItemStack &result, int32_t cost,
                           int32_t requiredLevel)
            : mPlayer(player), mInput(input), mResult(result), mCost(cost), mRequiredLevel(requiredLevel) {}
};

// an item about to lose durability; the player is null for the armor of a mob
struct ItemDamageBeforeEvent : public CancellableEvent {
    ServerPlayer *mPlayer;
    const ItemStack &mItem;
    int32_t mDamage;

    ItemDamageBeforeEvent(ServerPlayer *player, const ItemStack &item, int32_t damage)
            : mPlayer(player), mItem(item), mDamage(damage) {}
};

// an item out of durability, about to be removed
struct ItemBreakAfterEvent : public Event {
    ServerPlayer *mPlayer;
    const ItemStack &mItem;

    ItemBreakAfterEvent(ServerPlayer *player, const ItemStack &item) : mPlayer(player), mItem(item) {}
};

struct InventoryBeforeSignals {
    EventSignal<FurnaceBurnBeforeEvent> mFurnaceBurn;
    EventSignal<FurnaceSmeltBeforeEvent> mFurnaceSmelt;
    EventSignal<BlockContainerOpenBeforeEvent> mBlockContainerOpen;
    EventSignal<InventoryRequestBeforeEvent> mInventoryRequest;
    EventSignal<PlayerItemHeldBeforeEvent> mPlayerItemHeld;
    EventSignal<PlayerDropItemBeforeEvent> mPlayerDropItem;
    EventSignal<EnchantingOptionsBeforeEvent> mEnchantingOptions;
    EventSignal<PlayerInteractEntityBeforeEvent> mPlayerInteractEntity;
    EventSignal<PlayerPickupItemBeforeEvent> mPlayerPickupItem;
    EventSignal<ItemMergeBeforeEvent> mItemMerge;
    EventSignal<ItemDespawnBeforeEvent> mItemDespawn;
    EventSignal<PlayerItemUseBeforeEvent> mPlayerItemUse;
    EventSignal<PlayerItemConsumeBeforeEvent> mPlayerItemConsume;
    EventSignal<PlayerEditBookBeforeEvent> mPlayerEditBook;
    EventSignal<CraftItemBeforeEvent> mCraftItem;
    EventSignal<ItemEnchantBeforeEvent> mItemEnchant;
    EventSignal<ItemDamageBeforeEvent> mItemDamage;

    void clearInventoryBeforeSignals() {
        mFurnaceBurn.clear();
        mFurnaceSmelt.clear();
        mBlockContainerOpen.clear();
        mInventoryRequest.clear();
        mPlayerItemHeld.clear();
        mPlayerDropItem.clear();
        mEnchantingOptions.clear();
        mPlayerInteractEntity.clear();
        mPlayerPickupItem.clear();
        mItemMerge.clear();
        mItemDespawn.clear();
        mPlayerItemUse.clear();
        mPlayerItemConsume.clear();
        mPlayerEditBook.clear();
        mCraftItem.clear();
        mItemEnchant.clear();
        mItemDamage.clear();
    }
};

struct InventoryAfterSignals {
    EventSignal<BlockContainerCloseAfterEvent> mBlockContainerClose;
    EventSignal<AnvilUseAfterEvent> mAnvilUse;
    EventSignal<PlayerArmorChangeAfterEvent> mPlayerArmorChange;
    EventSignal<ItemBreakAfterEvent> mItemBreak;

    void clearInventoryAfterSignals() {
        mBlockContainerClose.clear();
        mAnvilUse.clear();
        mPlayerArmorChange.clear();
        mItemBreak.clear();
    }
};
