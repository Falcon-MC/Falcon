#pragma once

#include "Core/Utility/UUID.h"
#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

class ResourcePackChunkDataPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ResourcePackChunkData;

    ResourcePackChunkDataPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ResourcePackChunkDataPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Uuid mPackId;
    std::string mPackVersion;
    int32_t mChunkIndex = 0;
    int64_t mProgress = 0;
    std::string mData;
};
