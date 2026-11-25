#include "World/Actor/Vehicle/VehicleChest.h"

#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Inventory/ItemStackNbt.h"
#include "World/Level/Storage/Loot/LootItems.h"
#include "World/Level/Storage/Loot/LootTable.h"
#include "World/Level/Storage/Loot/LootTableRegistry.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Core/Json/Json.h"

#include <random>
#include <vector>

const char *VehicleChest::TAG_CHEST_ITEMS = "ChestItems";

namespace {
    const char *const TAG_ITEMS = "Items";

    // minecraft:inventory inventory_size of the bedrock-data definition
    int inventorySizeOf(const std::string &identifier) {
        const json::Value *definition = EntityDefinitions::find(identifier);
        const json::Value *components = definition == nullptr ? nullptr : definition->get("components");
        const json::Value *inventory = components == nullptr ? nullptr : components->get("minecraft:inventory");
        const json::Value *size = inventory == nullptr ? nullptr : inventory->get("inventory_size");
        // both chest vehicles define 27; the same before the definitions load
        return size != nullptr && size->isNumber() ? (int) size->mNumber : 27;
    }

    ContainerType containerTypeOf(const std::string &identifier) {
        return identifier == "minecraft:chest_boat" ? ContainerType::ChestBoat : ContainerType::MinecartChest;
    }
}

VehicleChest::VehicleChest(const std::string &identifier)
        : mIdentifier(identifier), mInventory(inventorySizeOf(identifier)) {}

bool VehicleChest::open(ServerNetworkHandler &owner, ServerPlayer &player, ServerActor &vehicle) {
    _unpackLoot(owner);
    return player.getInventoryManager().openEntityContainer(vehicle, containerTypeOf(mIdentifier));
}

void VehicleChest::dropAll(ServerNetworkHandler &owner, ServerActor &vehicle) {
    _unpackLoot(owner);
    Level &level = owner.getLevelFor(vehicle);
    for (int slot = 0; slot < mInventory.getContainerSize(); ++slot) {
        ItemStack item = mInventory.getContainerItem(slot);
        if (item.isAir())
            continue;
        mInventory.setContainerItem(slot, ItemStack::air());
        owner.dropItem(level, vehicle.getPosition(), item, ItemActorHandler::randomDropMotion(),
                       ItemActorHandler::DROP_PICKUP_DELAY);
    }
}

void VehicleChest::save(Tag &data) const {
    std::vector<Tag> items;
    for (int slot = 0; slot < mInventory.getContainerSize(); ++slot) {
        const ItemStack &item = mInventory.getContainerItem(slot);
        if (!item.isAir())
            items.push_back(ItemStackNbt::write(item, slot));
    }
    data.put(TAG_CHEST_ITEMS, Tag::ofList(Tag::Type::Compound, std::move(items)));
    if (!mLootTable.empty()) {
        data.putString("LootTable", mLootTable);
        data.putInt("LootTableSeed", mLootSeed);
    }
}

// worlds may carry Items instead of ChestItems
void VehicleChest::load(const Tag &data, const PacketCodecContext &context) {
    mLootTable = data.getString("LootTable", std::string());
    mLootSeed = data.getInt("LootTableSeed", 0);
    const Tag *items = data.get(TAG_CHEST_ITEMS);
    if (items == nullptr || items->getType() != Tag::Type::List)
        items = data.get(TAG_ITEMS);
    if (items == nullptr || items->getType() != Tag::Type::List)
        return;
    for (const Tag &entry: items->getList()) {
        const int slot = ItemStackNbt::readSlot(entry);
        if (slot >= 0 && slot < mInventory.getContainerSize())
            mInventory.setContainerItem(slot, ItemStackNbt::read(entry, context));
    }
}

void VehicleChest::_unpackLoot(ServerNetworkHandler &owner) {
    if (mLootTable.empty())
        return;

    const LootTable *table = LootTableRegistry::getInstance().get(mLootTable);
    if (table == nullptr)
        return;

    std::mt19937 random(mLootSeed != 0 ? (uint32_t) mLootSeed : std::random_device{}());
    LootContext context(random);
    LootItems::fillContainer(owner, mInventory, table->roll(context), random);
    mLootTable.clear();
}
