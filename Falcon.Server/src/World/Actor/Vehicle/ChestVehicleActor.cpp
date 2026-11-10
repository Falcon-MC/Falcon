#include "World/Actor/Vehicle/ChestVehicleActor.h"

#include "World/Actor/ActorFlags.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Inventory/ItemStackNbt.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Core/Json/Json.h"

const char *ChestVehicleActor::TAG_CHEST_ITEMS = "ChestItems";

namespace {
    const char *const TAG_ITEMS = "Items";

    // minecraft:inventory inventory_size of the bedrock-data definition
    int inventorySizeOf(const std::string &identifier) {
        const json::Value *definition = EntityDefinitions::find(identifier);
        const json::Value *components = definition == nullptr ? nullptr : definition->get("components");
        const json::Value *inventory = components == nullptr ? nullptr : components->get("minecraft:inventory");
        const json::Value *size = inventory == nullptr ? nullptr : inventory->get("inventory_size");
        // TODO: the size before the definitions load is unverified (IDA unavailable): the chest's 27
        return size != nullptr && size->isNumber() ? (int) size->mNumber : 27;
    }

    ContainerType containerTypeOf(const std::string &identifier) {
        return identifier == "minecraft:chest_boat" ? ContainerType::ChestBoat : ContainerType::MinecartChest;
    }
}

ChestVehicleActor::ChestVehicleActor(uint64_t runtimeId, const std::string &identifier, const ActorSize &size)
        : SizedActor(runtimeId, identifier, size, false), mInventory(inventorySizeOf(identifier)) {}

// a chest boat is boarded on a plain click, its chest opens on a sneaking one; a chest minecart always opens
bool ChestVehicleActor::onInteract(ServerNetworkHandler &owner, ServerPlayer &player) {
    (void) owner;
    const bool boat = getTypeId() == "minecraft:chest_boat";
    if (boat && !player.getFlags().get(ActorFlag::Sneaking))
        return false;
    return player.getInventoryManager().openEntityContainer(*this, containerTypeOf(getTypeId()));
}

void ChestVehicleActor::kill(ServerNetworkHandler &owner, ServerPlayer *source, int32_t lootingLevel) {
    if (isDead())
        return;
    Level &level = owner.getLevelFor(*this);
    for (int slot = 0; slot < mInventory.getContainerSize(); ++slot) {
        ItemStack item = mInventory.getContainerItem(slot);
        if (item.isAir())
            continue;
        mInventory.setContainerItem(slot, ItemStack::air());
        owner.dropItem(level, getPosition(), item, ItemActorHandler::randomDropMotion(),
                       ItemActorHandler::DROP_PICKUP_DELAY);
    }
    SizedActor::kill(owner, source, lootingLevel);
}

Tag ChestVehicleActor::saveNbt() const {
    Tag data = SizedActor::saveNbt();
    std::vector<Tag> items;
    for (int slot = 0; slot < mInventory.getContainerSize(); ++slot) {
        const ItemStack &item = mInventory.getContainerItem(slot);
        if (!item.isAir())
            items.push_back(ItemStackNbt::write(item, slot));
    }
    data.put(TAG_CHEST_ITEMS, Tag::ofList(Tag::Type::Compound, std::move(items)));
    return data;
}

// worlds may carry Items instead of ChestItems
void ChestVehicleActor::loadEquipment(const Tag &data, const PacketCodecContext &context) {
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
