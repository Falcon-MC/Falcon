#pragma once

#include "Core/NBT/Tag.h"
#include "World/Level/Block/Inventory/SimpleContainerInventory.h"

#include <cstdint>
#include <string>

class PacketCodecContext;
class ServerActor;
class ServerNetworkHandler;
class ServerPlayer;

// the 27 slots a chest minecart or a chest boat carries, with the loot table it fills them from when first opened
class VehicleChest {
public:
    static const char *TAG_CHEST_ITEMS;

    explicit VehicleChest(const std::string &identifier);

    Container &getContainer() {
        return mInventory;
    }

    bool open(ServerNetworkHandler &owner, ServerPlayer &player, ServerActor &vehicle);

    // drops every item where the vehicle stands
    void dropAll(ServerNetworkHandler &owner, ServerActor &vehicle);

    void save(Tag &data) const;

    void load(const Tag &data, const PacketCodecContext &context);

private:
    void _unpackLoot(ServerNetworkHandler &owner);

    std::string mIdentifier;
    std::string mLootTable;
    int32_t mLootSeed = 0;
    SimpleContainerInventory mInventory;
};
