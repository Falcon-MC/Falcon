#include "Network/Handler/ServerNetworkHandler.h"

#include "Actor/ActorFlags.h"
#include "Actor/Movement/RideControlSystem.h"
#include "Actor/ServerPlayer.h"
#include "Core/Event/GameEvents.h"
#include "Inventory/InventoryManager.h"
#include "Item/Item.h"
#include "Item/ItemData.h"
#include "Item/ItemNetworkIdTable.h"
#include "Item/VanillaItems.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/InventoryHandler.h"
#include "Plugin/PluginManager.h"
#include "Protocol/Packets/ActorEventPacket.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"

#include <string>

namespace {
    const int64_t DEFAULT_USE_DURATION_TICKS = 32;
    const int64_t EARLY_CONSUMABLE_RELEASE_BLOCK_TICKS = 10;
    const int64_t ITEM_USE_DEBOUNCE_TICKS = 4;
    const int64_t EATING_EVENT_INTERVAL_TICKS = 4;

    const Tag *vanillaComponents(const ItemStack &item) {
        if (item.isAir() || item.mDefinition == nullptr)
            return nullptr;

        const ItemNetworkIdEntry *entry = ItemNetworkIdTable::find(item.mDefinition->getIdentifier());
        return entry == nullptr ? nullptr : entry->mComponents.get("components");
    }

    const Item *itemTypeOf(const ItemStack &item) {
        if (item.isAir() || item.mDefinition == nullptr)
            return nullptr;
        return VanillaItems::fromIdentifier(item.mDefinition->getIdentifier());
    }

    int64_t useDurationTicksFor(const ItemStack &item) {
        const Tag *components = vanillaComponents(item);
        const Tag *duration = components == nullptr ? nullptr : components->get("minecraft:use_duration");
        if (duration != nullptr && duration->asInt() > 0)
            return duration->asInt();
        return DEFAULT_USE_DURATION_TICKS;
    }

    const FoodItemComponent *findFoodComponent(const ItemStack &item) {
        if (item.isAir() || item.mDefinition == nullptr)
            return nullptr;

        const ItemComponents &components = ItemDataTable::getComponents(item.mDefinition->getIdentifier());
        const FoodItemComponent *food = components.get<FoodItemComponent>();
        if (food == nullptr || !food->isEdible())
            return nullptr;

        return food;
    }

    bool isConsumable(const ItemStack &item) {
        if (findFoodComponent(item) != nullptr)
            return true;

        const Item *itemType = itemTypeOf(item);
        return itemType != nullptr && itemType->isConsumable();
    }

    bool canAlwaysConsume(const ItemStack &item) {
        const FoodItemComponent *food = findFoodComponent(item);
        if (food == nullptr || food->canAlwaysEat())
            return true;

        const Item *itemType = itemTypeOf(item);
        return itemType != nullptr && itemType->canAlwaysEat();
    }

    std::string usingConvertsTo(const ItemStack &item) {
        const Tag *components = vanillaComponents(item);
        const Tag *food = components == nullptr ? nullptr : components->get("minecraft:food");
        std::string name = food == nullptr ? std::string() : food->getString("using_converts_to");
        if (name.empty()) {
            const Item *itemType = itemTypeOf(item);
            name = itemType == nullptr ? std::string() : itemType->getUsingConvertsTo();
        }

        if (name.empty() || name.find(':') != std::string::npos)
            return name;
        return "minecraft:" + name;
    }

}

void ServerNetworkHandler::addEffect(Actor &actor, MobEffectId effect, int32_t amplifier, int32_t durationTicks) {
    MobEffectInstance instance;
    instance.mId = effect;
    instance.mAmplifier = amplifier;
    instance.mDuration = durationTicks;

    if (!actor.addEffect(instance) || !actor.isPlayer())
        return;

    EffectAddAfterEvent event(static_cast<ServerPlayer &>(actor), (int32_t) effect, amplifier, durationTicks);
    mEventBus.after().mEffectAdd.emit(event);
}

void ServerNetworkHandler::_tickItemUse(ServerPlayer &player) {
    if (!player.isSpawned() || !player.getFlags().get(ActorFlag::UsingItem))
        return;

    const int64_t itemUseTicks = mCurrentTick - player.getItemUseStartTick();
    const ItemStack &item = player.getInventory().getItemInHand();
    const bool consumable = isConsumable(item);

    if (consumable && itemUseTicks >= useDurationTicksFor(item)) {
        _consumeHeldItem(player);
        return;
    }

    if (consumable && itemUseTicks > 0 && itemUseTicks % EATING_EVENT_INTERVAL_TICKS == 0
        && item.mDefinition != nullptr) {
        const int32_t eventData = (int32_t) (((item.mDefinition->getRuntimeId() & 0xffff) << 16)
                                             | (item.mDamage & 0xffff));
        _broadcastEntityEvent(player, (uint8_t) EntityEventType::EatingItem, eventData);
        return;
    }

    if (consumable || item.mDefinition == nullptr)
        return;

    const Item *itemType = VanillaItems::fromIdentifier(item.mDefinition->getIdentifier());
    if (itemType != nullptr)
        itemType->onUsingTick(*this, player, item, (int32_t) itemUseTicks);
}

void ServerNetworkHandler::_completeItemUse(ServerPlayer &player, int32_t itemId) {
    if (!player.getFlags().get(ActorFlag::UsingItem))
        return;

    const ItemStack &heldItem = player.getInventory().getItemInHand();
    const int64_t heldTicks = mCurrentTick - player.getItemUseStartTick();
    if (heldTicks < useDurationTicksFor(heldItem))
        return;

    if (heldItem.isAir() || heldItem.mDefinition == nullptr ||
        static_cast<uint16_t>(itemId) != static_cast<uint16_t>(heldItem.mDefinition->getRuntimeId()))
        return;

    _consumeHeldItem(player);
}

void ServerNetworkHandler::emitItemUse(ServerPlayer &player) {
    const ItemStack &heldItem = player.getInventory().getItemInHand();
    if (heldItem.isAir() || heldItem.mDefinition == nullptr)
        return;

    if (mCurrentTick - player.getLastItemUseTick() < ITEM_USE_DEBOUNCE_TICKS)
        return;
    player.setLastItemUseTick(mCurrentTick);

    ItemUseAfterEvent useEvent(player, heldItem.mDefinition->getIdentifier());
    mEventBus.after().mItemUse.emit(useEvent);
}

bool ServerNetworkHandler::_equipHeldArmor(ServerPlayer &player, const Item &itemType) {
    const ArmorSlot slot = itemType.getArmorSlot();
    if (slot == ArmorSlot::None)
        return false;

    int armorSlot = PlayerInventory::ARMOR_HEAD;
    switch (slot) {
        case ArmorSlot::Head:
            armorSlot = PlayerInventory::ARMOR_HEAD;
            break;
        case ArmorSlot::Chest:
            armorSlot = PlayerInventory::ARMOR_TORSO;
            break;
        case ArmorSlot::Legs:
            armorSlot = PlayerInventory::ARMOR_LEGS;
            break;
        case ArmorSlot::Feet:
            armorSlot = PlayerInventory::ARMOR_FEET;
            break;
        default:
            return false;
    }

    PlayerInventory &inventory = player.getInventory();

    ItemStack worn = inventory.getArmor(armorSlot);
    ItemStack held = inventory.getItemInHand();

    inventory.setArmor(armorSlot, held);
    inventory.setItemInHand(std::move(worn));

    player.getInventoryManager().syncContents(InventoryManager::InventoryId::Armor);
    player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                          inventory.getSelectedSlot());
    InventoryHandler::sendArmorContent(*this, player);
    InventoryHandler::sendHeldItem(*this, player);

    playLevelSound(getLevelFor(player), LevelSoundEvent::ARMOR_EQUIP_GENERIC, player.getPosition(),
                   "minecraft:player");
    return true;
}

void ServerNetworkHandler::_useHeldItem(ServerPlayer &player) {
    if (mScriptEngine.beforeItemUse(player)) {
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                              player.getInventory().getSelectedSlot());
        return;
    }

    const ItemStack &heldItem = player.getInventory().getItemInHand();

    emitItemUse(player);

    if (player.isRiding() && RideControlSystem::tryBoost(*this, player))
        return;

    if (!heldItem.isAir() && heldItem.mDefinition != nullptr) {
        const Item *itemType = VanillaItems::fromIdentifier(heldItem.mDefinition->getIdentifier());
        if (itemType != nullptr && _equipHeldArmor(player, *itemType))
            return;

        if (itemType != nullptr && itemType->onUse(*this, player, heldItem))
            return;

        if (itemType != nullptr && !player.getFlags().get(ActorFlag::UsingItem)
            && itemType->onStartUsing(*this, player, heldItem)) {
            player.getFlags().set(ActorFlag::UsingItem, true);
            player.setItemUseStartTick(mCurrentTick);
            _sendEntityData(player);
            return;
        }
    }

    if (!isConsumable(heldItem))
        return;

    const Item *consumedType = itemTypeOf(heldItem);
    if (consumedType != nullptr && !consumedType->canConsume(*this, player))
        return;

    if (player.isAwaitingConsumableRelease())
        return;

    if (mCurrentTick - player.getLastEarlyConsumableReleaseTick() < EARLY_CONSUMABLE_RELEASE_BLOCK_TICKS)
        return;

    if (player.hasItemCooldown(heldItem, mCurrentTick))
        return;

    const bool usingItem = player.getFlags().get(ActorFlag::UsingItem);

    if (!canAlwaysConsume(heldItem) && !player.canEat()) {
        if (usingItem) {
            player.getFlags().set(ActorFlag::UsingItem, false);
            _sendEntityData(player);
        }
        return;
    }

    if (usingItem)
        return;

    player.getFlags().set(ActorFlag::UsingItem, true);
    player.setItemUseStartTick(mCurrentTick);
    _sendEntityData(player);

    if (heldItem.mDefinition != nullptr) {
        ItemStartUseAfterEvent event(player, heldItem.mDefinition->getIdentifier(),
                                     (int32_t) useDurationTicksFor(heldItem));
        mEventBus.after().mItemStartUse.emit(event);
    }
}

void ServerNetworkHandler::_consumeHeldItem(ServerPlayer &player) {
    const bool wasUsing = player.getFlags().get(ActorFlag::UsingItem);
    const int64_t heldTicks = mCurrentTick - player.getItemUseStartTick();

    PlayerInventory &inventory = player.getInventory();
    const int slot = inventory.getSelectedSlot();

    const ItemStack &heldItem = inventory.getItemInHand();
    const FoodItemComponent *food = findFoodComponent(heldItem);
    const Item *itemType = itemTypeOf(heldItem);
    const bool consumable = isConsumable(heldItem);

    if (wasUsing && consumable && heldTicks < useDurationTicksFor(heldItem))
        return;

    if (wasUsing) {
        player.getFlags().set(ActorFlag::UsingItem, false);
        _sendEntityData(player);
    }

    if (!consumable)
        return;

    if (itemType != nullptr && !itemType->canConsume(*this, player)) {
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
        return;
    }

    const ItemStack usedItem = heldItem;

    if (!wasUsing) {
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
        return;
    }

    if (!canAlwaysConsume(usedItem) && !player.canEat()) {
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
        _sendAttributes(player);
        return;
    }

    if (PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_PLAYER_ITEM_CONSUME)) {
        ItemStack consumed = usedItem;
        PluginEvent consumeEvent;
        consumeEvent.mType = FALCON_EVENT_PLAYER_ITEM_CONSUME;
        consumeEvent.mCancellable = true;
        consumeEvent.mPlayer = &player;
        consumeEvent.mItem = &consumed;
        plugins->dispatch(consumeEvent);
        if (consumeEvent.mCancelled) {
            player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
            _sendAttributes(player);
            return;
        }
    }

    if (usedItem.mDefinition != nullptr) {
        const std::string identifier = usedItem.mDefinition->getIdentifier();
        ItemCompleteUseAfterEvent completeEvent(player, identifier, (int32_t) useDurationTicksFor(usedItem));
        mEventBus.after().mItemCompleteUse.emit(completeEvent);
    }

    if (food != nullptr) {
        player.consumeFood(food->getNutrition(), food->getSaturation());
        for (const FoodEffect &effect: food->getEffects())
            addEffect(player, (MobEffectId) effect.mEffectId, effect.mAmplifier, effect.mDurationTicks);
    }

    if (itemType != nullptr)
        itemType->onConsumed(*this, player, usedItem);

    if (food != nullptr)
        playLevelSound(getLevelFor(player), LevelSoundEvent::BURP, player.getPosition());

    ItemStack remaining = inventory.getItemInHand();
    remaining.mCount -= 1;
    if (remaining.mCount <= 0) {
        const std::string convertsTo = usingConvertsTo(usedItem);
        remaining = ItemStack::air();
        if (!convertsTo.empty()) {
            remaining.mDefinition = mItemDefinitions.getDefinition(convertsTo);
            remaining.mBlockDefinition = mBlockDefinitions.getDefinition(convertsTo);
            remaining.mCount = remaining.mDefinition == nullptr ? 0 : 1;
        }
    }

    inventory.setItemInHand(std::move(remaining));
    player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);

    player.startItemCooldown(usedItem, mCurrentTick);
    player.setAwaitingConsumableRelease();

    _sendAttributes(player);
}
