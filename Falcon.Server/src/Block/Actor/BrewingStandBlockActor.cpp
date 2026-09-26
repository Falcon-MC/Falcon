#include "Block/Actor/ContainerBlockActor.h"

#include "Actor/ServerPlayer.h"
#include "Inventory/InventoryManager.h"
#include "Item/CraftingRecipeTable.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/NetworkHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/ContainerSetDataPacket.h"
#include "Protocol/Types/ItemDefinition.h"

#include <memory>
#include <string>
#include <utility>

namespace {
    const char *TAG_COOK_TIME = "CookTime";
    const char *TAG_FUEL_AMOUNT = "FuelAmount";
    const char *TAG_FUEL_TOTAL = "FuelTotal";
    const char *BLAZE_POWDER = "minecraft:blaze_powder";
    const char *BREWED_SOUND = "potion.brewed";
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

    Vector3f centerOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }
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
    const int brewTime = data.contains(TAG_COOK_TIME) ? data.getShort(TAG_COOK_TIME) : MAX_BREW_TIME;
    mBrewTime = brewTime > MAX_BREW_TIME ? MAX_BREW_TIME : brewTime;
    mFuelAmount = data.getShort(TAG_FUEL_AMOUNT);
    mFuelTotal = data.getShort(TAG_FUEL_TOTAL);
}

Tag BrewingStandBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    data.putShort(TAG_FUEL_TOTAL, (int16_t) mFuelTotal);
    data.putShort(TAG_FUEL_AMOUNT, (int16_t) mFuelAmount);
    if (mBrewTime < MAX_BREW_TIME)
        data.putShort(TAG_COOK_TIME, (int16_t) mBrewTime);
    return data;
}

bool BrewingStandBlockActor::tick(ServerNetworkHandler &owner) {
    if (mLevel == nullptr || mLevel->peekBlockPtr(mPosition.x, mPosition.y, mPosition.z) == nullptr)
        return true;

    _updateSlotStates(owner);
    _restockFuel(owner);

    if (mFuelAmount <= 0 || !_hasMix()) {
        if (mBrewTime != MAX_BREW_TIME)
            _stopBrewing(owner);
        return true;
    }

    if (mBrewTime == MAX_BREW_TIME)
        _sendProperty(owner, PROPERTY_BREW_TIME, mBrewTime);

    if (--mBrewTime > 0) {
        if (mBrewTime % BREW_TIME_SYNC_INTERVAL == 0)
            _sendProperty(owner, PROPERTY_BREW_TIME, mBrewTime);
        return true;
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
    if (identifierOf(fuel) != BLAZE_POWDER || fuel.mCount <= 0)
        return;

    --fuel.mCount;
    if (fuel.mCount <= 0)
        fuel = ItemStack::air();
    mInventory.setContainerItem(SLOT_FUEL, std::move(fuel));

    mFuelAmount = FUEL_PER_BLAZE_POWDER;
    mFuelTotal = FUEL_PER_BLAZE_POWDER;
    _sendProperty(owner, PROPERTY_FUEL_AMOUNT, mFuelAmount);
    _sendProperty(owner, PROPERTY_FUEL_TOTAL, mFuelTotal);
    owner.refreshContainerViewers(mPosition, nullptr);
}

void BrewingStandBlockActor::_brew(ServerNetworkHandler &owner) {
    ItemStack ingredient = mInventory.getContainerItem(SLOT_INGREDIENT);
    const std::string reagentId = identifierOf(ingredient);
    if (reagentId.empty())
        return;

    bool mixed = false;
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
        mInventory.setContainerItem(slot, std::move(result));
        mixed = true;
    }

    if (!mixed)
        return;

    --ingredient.mCount;
    if (ingredient.mCount <= 0)
        ingredient = ItemStack::air();
    mInventory.setContainerItem(SLOT_INGREDIENT, std::move(ingredient));

    --mFuelAmount;
    _sendProperty(owner, PROPERTY_FUEL_AMOUNT, mFuelAmount);
    _sendProperty(owner, PROPERTY_FUEL_TOTAL, mFuelTotal);
    owner.refreshContainerViewers(mPosition, nullptr);
    owner.playLevelSound(*mLevel, BREWED_SOUND, centerOf(mPosition));
}

void BrewingStandBlockActor::_stopBrewing(ServerNetworkHandler &owner) {
    mBrewTime = 0;
    _sendProperty(owner, PROPERTY_BREW_TIME, mBrewTime);
    mBrewTime = MAX_BREW_TIME;
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
