#pragma once

#include "Protocol/Packet.h"

class NetworkStackLatencyPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::NetworkStackLatency;

    NetworkStackLatencyPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "NetworkStackLatencyPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint64_t mTimestamp;
    bool mFromServer;
};
