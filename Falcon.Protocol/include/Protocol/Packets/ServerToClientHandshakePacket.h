#pragma once

#include "Protocol/Packet.h"

class ServerToClientHandshakePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ServerToClientHandshake;

    ServerToClientHandshakePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ServerToClientHandshakePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mJwt;
};
