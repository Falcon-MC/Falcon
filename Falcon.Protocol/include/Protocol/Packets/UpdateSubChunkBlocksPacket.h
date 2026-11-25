#pragma once

#include "Core/Math/Vector3i.h"
#include "Protocol/Packet.h"
#include "Protocol/Types/BlockChangeEntry.h"

#include <vector>

class UpdateSubChunkBlocksPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::UpdateSubChunkBlocks;

    UpdateSubChunkBlocksPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "UpdateSubChunkBlocksPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Vector3i mPosition;
    std::vector<BlockChangeEntry> mStandardBlocks;
    std::vector<BlockChangeEntry> mExtraBlocks;
};
