#pragma once

#include "Protocol/Packet.h"

class HurtArmorPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::HurtArmor;

    HurtArmorPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "HurtArmorPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int32_t mCause;
    int32_t mDamage;
    uint64_t mArmorSlots;
};
