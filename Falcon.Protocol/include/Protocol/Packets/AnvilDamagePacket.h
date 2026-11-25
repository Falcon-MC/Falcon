#pragma once

#include "Protocol/Packet.h"

class AnvilDamagePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::AnvilDamage;

    AnvilDamagePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "AnvilDamagePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Vector3i mPosition;
};
