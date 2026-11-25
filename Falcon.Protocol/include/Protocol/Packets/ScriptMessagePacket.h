#pragma once

#include "Protocol/Packet.h"

class ScriptMessagePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ScriptMessage;

    ScriptMessagePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ScriptMessagePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mChannel;
    std::string mMessage;
};
