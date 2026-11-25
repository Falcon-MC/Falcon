#pragma once

#include "Protocol/Packet.h"

#include <cstdint>

class PhotoInfoRequestPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::PhotoInfoRequest;

    PhotoInfoRequestPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "PhotoInfoRequestPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mPhotoId = 0;
};
