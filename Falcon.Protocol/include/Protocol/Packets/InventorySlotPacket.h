#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/FullContainerName.h"
#include "Protocol/Types/ItemStack.h"

#include <cstdint>

class InventorySlotPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::InventorySlot;

    InventorySlotPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "InventorySlotPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int32_t mContainerId = 0;
    int32_t mSlot = 0;
    ItemStack mItem;

    bool mHasContainerNameData = false;
    FullContainerName mContainerNameData;

    bool mHasStorageItem = false;
    ItemStack mStorageItem;
};
