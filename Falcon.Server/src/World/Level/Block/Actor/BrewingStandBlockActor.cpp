#include "World/Level/Block/Actor/ContainerBlockActor.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/ContainerBlock.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Item/CraftingRecipeTable.h"
#include "World/Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/NetworkHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/ContainerSetDataPacket.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Types/ItemDefinition.h"
#include "Core/Math/VectorMath.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace {
    const char *TAG_COOK_TIME = "CookTime";
    const char *TAG_FUEL_AMOUNT = "FuelAmount";
    const char *TAG_FUEL_TOTAL = "FuelTotal";
    const char *BLAZE_POWDER = "minecraft:blaze_powder";
    const char *const SLOT_STATES[] = {"brewing_stand_slot_a_bit", "brewing_stand_slot_b_bit",
                                       "brewing_stand_slot_c_bit"};
    const int PROPERTY_BREW_TIME = 0;
    const int PROPERTY_FUEL_AMOUNT = 1;
    const int PROPERTY_FUEL_TOTAL = 2;
    const int BREW_TIME_SYNC_INTERVAL = 40;

    std::string identifierOf(const ItemStack &item) {
        if (item.isAir() || item.mDefinition == nullptr)
            return std::string();

        return std::string(item.mDefinition->getIdentifier());
    }

    bool isPotion(const std::string &identifier) {
        return identifier == "minecraft:potion" || identifier == "minecraft:splash_potion"
               || identifier == "minecraft:lingering_potion";
    }

    using VectorMath::centerOf;
}

Tag BrewingStandBlockActor::saveNbt() const {
    Tag data = ContainerBlockActor::saveNbt();
    data.putShort(TAG_COOK_TIME, (int16_t) mBrewTime);
    data.putShort(TAG_FUEL_AMOUNT, (int16_t) mFuelAmount);
    data.putShort(TAG_FUEL_TOTAL, (int16_t) mFuelTotal);
    return data;
}

void BrewingStandBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    ContainerBlockActor::loadNbt(data, context);
    mBrewTime = std::clamp((int) data.getShort(TAG_COOK_TIME), 0, MAX_BREW_TIME);
    mFuelAmount = data.getShort(TAG_FUEL_AMOUNT);
    mFuelTotal = data.getShort(TAG_FUEL_TOTAL);
    mBrewingIngredient = mBrewTime > 0 ? mInventory.getContainerItem(SLOT_INGREDIENT).mDefinition : nullptr;
}

Tag BrewingStandBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    data.putShort(TAG_FUEL_TOTAL, (int16_t) mFuelTotal);
    data.putShort(TAG_FUEL_AMOUNT, (int16_t) mFuelAmount);
    if (mBrewTime > 0)
        data.putShort(TAG_COOK_TIME, (int16_t) mBrewTime);
    return data;
}

bool BrewingStandBlockActor::tick(ServerNetworkHandler &owner) {
    if (mLevel == nullptr)
        return true;

    const BlockState *state = mLevel->peekBlockPtr(mPosition.x, mPosition.y, mPosition.z);
    if (state == nullptr)
        return true;

    const ContainerBlockDefinition *definition = ContainerBlock::findDefinition(state->mName);
    if (definition == nullptr || definition->mKind != ContainerBlockKind::BrewingStand)
        return false;

    _updateSlotStates(owner);
    _restockFuel(owner);

    const bool canBrew = mFuelAmount > 0 && _hasMix();
    const auto &ingredientType = mInventory.getContainerItem(SLOT_INGREDIENT).mDefinition;
    if (mBrewTime == 0) {
        if (canBrew) {
            mBrewTime = MAX_BREW_TIME;
            mBrewingIngredient = ingredientType;
            _sendProperty(owner, PROPERTY_BREW_TIME, mBrewTime);
        }
        return true;
    }

    --mBrewTime;
    // the completing tick uses the current recipe; ingredient changes cancel only while time remains
    if (!canBrew || (mBrewTime > 0 && mBrewingIngredient != ingredientType)) {
        _stopBrewing(owner);
        return true;
    }

    if (mBrewTime > 0) {
        if (mBrewTime % BREW_TIME_SYNC_INTERVAL == 0)
            _sendProperty(owner, PROPERTY_BREW_TIME, mBrewTime);
        return true;
    }

    EventSignal<BrewBeforeEvent> &brew = owner.getEventBus().before().mBrew;
    if (brew.hasSubscribers()) {
        BrewBeforeEvent event(*mLevel, mPosition, mInventory.getContainerItem(SLOT_INGREDIENT));
        brew.emit(event);
        if (event.isCancelled()) {
            _stopBrewing(owner);
            return true;
        }
    }

    _brew(owner);
    _stopBrewing(owner);
    return true;
}

bool BrewingStandBlockActor::_hasMix() const {
    const ItemStack &ingredient = mInventory.getContainerItem(SLOT_INGREDIENT);
    const std::string reagentId = identifierOf(ingredient);
    if (reagentId.empty())
        return false;

    for (int slot = FIRST_POTION_SLOT; slot < FIRST_POTION_SLOT + POTION_SLOTS; ++slot) {
        const ItemStack &potion = mInventory.getContainerItem(slot);
        const std::string potionId = identifierOf(potion);
        if (!potionId.empty()
            && CraftingRecipeTable::findBrewingMix(potionId, potion.mDamage, reagentId, ingredient.mDamage) != nullptr)
            return true;
    }
    return false;
}

void BrewingStandBlockActor::_restockFuel(ServerNetworkHandler &owner) {
    if (mFuelAmount > 0)
        return;

    ItemStack fuel = mInventory.getContainerItem(SLOT_FUEL);
    const bool validFuel = identifierOf(fuel) == BLAZE_POWDER && fuel.mCount > 0;
    int fuelTime = FUEL_PER_BLAZE_POWDER;
    EventSignal<BrewingFuelUseBeforeEvent> &fuelUse = owner.getEventBus().before().mBrewingFuelUse;
    if (fuelUse.hasSubscribers() && _hasMix()) {
        BrewingFuelUseBeforeEvent event(*mLevel, mPosition, fuel, fuelTime);
        event.setCancelled(!validFuel);
        fuelUse.emit(event);
        if (event.isCancelled())
            return;
        fuelTime = event.mFuelTime;
    } else if (!validFuel) {
        return;
    }

    --fuel.mCount;
    if (fuel.mCount <= 0)
        fuel = ItemStack::air();
    mInventory.setContainerItem(SLOT_FUEL, std::move(fuel));

    mFuelAmount = fuelTime;
    mFuelTotal = fuelTime;
    _sendProperty(owner, PROPERTY_FUEL_AMOUNT, mFuelAmount);
    _sendProperty(owner, PROPERTY_FUEL_TOTAL, mFuelTotal);
    owner.refreshContainerViewers(mPosition, nullptr);
}

void BrewingStandBlockActor::_brew(ServerNetworkHandler &owner) {
    ItemStack ingredient = mInventory.getContainerItem(SLOT_INGREDIENT);
    const std::string reagentId = identifierOf(ingredient);
    if (reagentId.empty())
        return;

    EventSignal<BrewItemBeforeEvent> &brewItem = owner.getEventBus().before().mBrewItem;
    bool mixed = false;
    bool tried = false;
    for (int slot = FIRST_POTION_SLOT; slot < FIRST_POTION_SLOT + POTION_SLOTS; ++slot) {
        const ItemStack &potion = mInventory.getContainerItem(slot);
        const std::string potionId = identifierOf(potion);
        if (potionId.empty())
            continue;

        const BrewingMixData *mix = CraftingRecipeTable::findBrewingMix(potionId, potion.mDamage, reagentId,
                                                                        ingredient.mDamage);
        if (mix == nullptr)
            continue;

        std::shared_ptr<ItemDefinition> output = owner.getItemDefinitions().getDefinition(mix->mOutputId);
        if (output == nullptr)
            continue;

        ItemStack result = potion;
        result.mDefinition = std::move(output);
        if (mix->mOutputMeta >= 0)
            result.mDamage = mix->mOutputMeta;
        tried = true;
        if (brewItem.hasSubscribers()) {
            BrewItemBeforeEvent event(*mLevel, mPosition, slot, potion, result);
            brewItem.emit(event);
            if (event.isCancelled())
                continue;
        }
        mInventory.setContainerItem(slot, std::move(result));
        mixed = true;
    }

    if (!tried)
        return;

    --ingredient.mCount;
    if (ingredient.mCount <= 0)
        ingredient = ItemStack::air();
    mInventory.setContainerItem(SLOT_INGREDIENT, std::move(ingredient));

    --mFuelAmount;
    _sendProperty(owner, PROPERTY_FUEL_AMOUNT, mFuelAmount);
    _sendProperty(owner, PROPERTY_FUEL_TOTAL, mFuelTotal);
    owner.refreshContainerViewers(mPosition, nullptr);
    if (mixed)
        owner.playLevelSound(*mLevel, LevelSoundEvent::POTION_BREWED, centerOf(mPosition));
}

void BrewingStandBlockActor::_stopBrewing(ServerNetworkHandler &owner) {
    mBrewTime = 0;
    mBrewingIngredient.reset();
    _sendProperty(owner, PROPERTY_BREW_TIME, mBrewTime);
}

void BrewingStandBlockActor::_sendProperty(ServerNetworkHandler &owner, int property, int value) const {
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        const InventoryManager &manager = player.getInventoryManager();
        if (!manager.isContainerOpen() || manager.getContainerPosition() != mPosition
            || &owner.getLevelFor(player) != mLevel)
            continue;

        ContainerSetDataPacket packet;
        packet.mWindowId = (int8_t) manager.getContainerWindowId();
        packet.mProperty = property;
        packet.mValue = value;
        owner.getNetworkHandler().send(entry.first, packet, owner.getCodecContext());
    }
}

void BrewingStandBlockActor::_updateSlotStates(ServerNetworkHandler &owner) {
    const BlockState *current = mLevel->peekBlockPtr(mPosition.x, mPosition.y, mPosition.z);
    if (current == nullptr)
        return;

    Tag states = current->mStates;
    bool changed = false;
    for (int index = 0; index < POTION_SLOTS; ++index) {
        if (!states.contains(SLOT_STATES[index]))
            continue;

        const bool filled = isPotion(identifierOf(mInventory.getContainerItem(FIRST_POTION_SLOT + index)));
        if ((states.getByte(SLOT_STATES[index]) != 0) == filled)
            continue;

        states.putByte(SLOT_STATES[index], filled ? 1 : 0);
        changed = true;
    }

    if (!changed)
        return;

    const BlockState updated(current->mName, states);
    mLevel->setBlockState(mPosition.x, mPosition.y, mPosition.z, updated);
    BlockActionHandler::broadcastBlockUpdate(owner, *mLevel, mPosition, updated);
}
