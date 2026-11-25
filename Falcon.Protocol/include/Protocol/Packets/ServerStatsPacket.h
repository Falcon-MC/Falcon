#pragma once

#include "Protocol/Packet.h"

class ServerStatsPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ServerStats;

    ServerStatsPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ServerStatsPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    float mServerTime;
    float mNetworkTime;
};
