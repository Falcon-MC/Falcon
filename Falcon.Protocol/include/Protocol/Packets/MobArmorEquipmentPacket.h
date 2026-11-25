#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/ItemStack.h"

#include <cstdint>

class MobArmorEquipmentPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::MobArmorEquipment;

    MobArmorEquipmentPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "MobArmorEquipmentPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mRuntimeActorId = 0;
    ItemStack mHelmet;
    ItemStack mChestplate;
    ItemStack mLeggings;
    ItemStack mBoots;
    ItemStack mBody;
};
