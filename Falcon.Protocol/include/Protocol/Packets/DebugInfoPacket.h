#pragma once

#include "Protocol/Packet.h"

class DebugInfoPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::DebugInfo;

    DebugInfoPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "DebugInfoPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mUniqueActorId;
    std::string mData;
};
