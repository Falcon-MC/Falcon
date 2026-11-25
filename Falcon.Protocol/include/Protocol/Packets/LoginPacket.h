#pragma once

#include "Protocol/Packet.h"

#include <string>

class LoginPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::Login;

    LoginPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "LoginPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int32_t mProtocolVersion;

    // raw json holding either a certificate chain or an authentication token
    std::string mAuthJwt;

    // jwt describing the client, skin and device
    std::string mClientJwt;
};
