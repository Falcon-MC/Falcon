#pragma once

#include "Protocol/Packet.h"

class ServerPlayerPostMovePositionPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ServerPlayerPostMovePosition;

    ServerPlayerPostMovePositionPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ServerPlayerPostMovePositionPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Vector3f mPosition;
};
