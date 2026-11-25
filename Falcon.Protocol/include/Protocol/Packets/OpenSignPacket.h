#pragma once

#include "Protocol/Packet.h"

class OpenSignPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::OpenSign;

    OpenSignPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "OpenSignPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Vector3i mPosition;
    bool mFrontSide;
};
