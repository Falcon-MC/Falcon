#pragma once

#include "Protocol/Packet.h"

#include <string>

class SubClientLoginPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::SubClientLogin;

    SubClientLoginPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "SubClientLoginPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    // raw json holding either a certificate chain or an authentication token
    std::string mAuthJwt;

    // jwt describing the client, skin and device
    std::string mClientJwt;
};
