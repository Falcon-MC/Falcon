#pragma once

#include "Protocol/Packet.h"

#include <cstdint>

enum class PositionTrackingDBRequestAction : unsigned char {
    Query = 0
};

class PositionTrackingDBClientRequestPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::PositionTrackingDBClientRequest;

    PositionTrackingDBClientRequestPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "PositionTrackingDBClientRequestPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    PositionTrackingDBRequestAction mAction = PositionTrackingDBRequestAction::Query;
    int32_t mTrackingId = 0;
};
