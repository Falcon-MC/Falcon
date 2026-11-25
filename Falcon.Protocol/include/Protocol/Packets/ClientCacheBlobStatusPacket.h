#pragma once

#include "Protocol/Packet.h"

#include <vector>

class ClientCacheBlobStatusPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ClientCacheBlobStatus;

    static const uint32_t MAX_BLOBS = 4095;

    ClientCacheBlobStatusPacket() = default;

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ClientCacheBlobStatusPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<uint64_t> mNaks;
    std::vector<uint64_t> mAcks;
};
