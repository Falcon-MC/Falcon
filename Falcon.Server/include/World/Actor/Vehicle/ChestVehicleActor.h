#pragma once

#include "World/Actor/SizedActor.h"
#include "World/Level/Block/Inventory/SimpleContainerInventory.h"

// chest minecart and chest boat: a SizedActor carrying the 27 slots of its minecraft:inventory
class ChestVehicleActor : public SizedActor {
public:
    static const char *TAG_CHEST_ITEMS;

    ChestVehicleActor(uint64_t runtimeId, const std::string &identifier, const ActorSize &size, bool minecart);

    bool isMinecart() const override {
        return mMinecart;
    }

    Container *getEntityContainer() override {
        return &mInventory;
    }

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player) override;

    void kill(ServerNetworkHandler &owner, ServerPlayer *source = nullptr, int32_t lootingLevel = 0) override;

    Tag saveNbt() const override;

    void loadEquipment(const Tag &data, const PacketCodecContext &context) override;

private:
    SimpleContainerInventory mInventory;
    bool mMinecart;
};
