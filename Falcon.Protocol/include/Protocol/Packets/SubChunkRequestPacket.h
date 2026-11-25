#pragma once

#include "Core/Math/Vector3i.h"
#include "Protocol/Packet.h"

#include <vector>

class SubChunkRequestPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::SubChunkRequest;

    // Vanilla clients can request every sub chunk within the maximum view distance
    static const uint32_t MAX_SUB_CHUNKS = 8192;

    SubChunkRequestPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "SubChunkRequestPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int32_t mDimension;
    Vector3i mSubChunkPosition;
    std::vector<Vector3i> mPositionOffsets;
};
