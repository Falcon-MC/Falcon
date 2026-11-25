#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/LocatorBarWaypoint.h"

#include <vector>

class LocatorBarPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::LocatorBar;

    LocatorBarPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "LocatorBarPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<LocatorBarPayload> mWaypoints;
};
