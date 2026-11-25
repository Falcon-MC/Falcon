#pragma once

#include "Protocol/Packet.h"

#include <cstdint>

class TickSyncPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::TickSync;

    TickSyncPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "TickSyncPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mRequestTimestamp = 0;
    int64_t mResponseTimestamp = 0;
};
