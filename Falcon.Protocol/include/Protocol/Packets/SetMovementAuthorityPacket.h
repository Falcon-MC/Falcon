#pragma once

#include "Protocol/Packet.h"

enum class AuthoritativeMovementMode : unsigned char {
    Client = 0,
    Server = 1,
    ServerWithRewind = 2
};

class SetMovementAuthorityPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::SetMovementAuthority;

    SetMovementAuthorityPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "SetMovementAuthorityPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    AuthoritativeMovementMode mMode;
};
