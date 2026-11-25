#pragma once

#include "Protocol/Packet.h"

class ChunkRadiusUpdatedPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ChunkRadiusUpdated;

    ChunkRadiusUpdatedPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ChunkRadiusUpdatedPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int32_t mRadius;
};
