#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/ItemStack.h"

#include <cstdint>

class MobEquipmentPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::MobEquipment;

    MobEquipmentPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "MobEquipmentPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mRuntimeActorId = 0;
    ItemStack mItem;
    int32_t mInventorySlot = 0;
    int32_t mHotbarSlot = 0;
    int32_t mContainerId = 0;
};
