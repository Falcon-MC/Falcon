#pragma once

#include "Protocol/Packet.h"

class SetCommandsEnabledPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::SetCommandsEnabled;

    SetCommandsEnabledPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "SetCommandsEnabledPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    bool mCommandsEnabled;
};
