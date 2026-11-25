#pragma once

#include "Protocol/Packet.h"

#include <string>
#include <vector>

class LevelChunkPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::LevelChunk;

    LevelChunkPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "LevelChunkPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int32_t mChunkX;
    int32_t mChunkZ;
    int32_t mDimension;
    uint32_t mSubChunksLength;
    bool mCachingEnabled;
    bool mRequestSubChunks;
    int32_t mSubChunkLimit;
    std::vector<uint64_t> mBlobIds;
    std::string mData;
    bool mClientBiomeUpdate;
};
