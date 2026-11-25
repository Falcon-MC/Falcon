#pragma once

#include "Core/NBT/Tag.h"
#include "Protocol/Packet.h"

#include <cstdint>

enum class PositionTrackingDBBroadcastAction : unsigned char {
    Update = 0,
    Destroy = 1,
    NotFound = 2
};

class PositionTrackingDBServerBroadcastPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::PositionTrackingDBServerBroadcast;

    PositionTrackingDBServerBroadcastPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "PositionTrackingDBServerBroadcastPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    PositionTrackingDBBroadcastAction mAction = PositionTrackingDBBroadcastAction::Update;
    int32_t mTrackingId = 0;
    Tag mTag;
};
