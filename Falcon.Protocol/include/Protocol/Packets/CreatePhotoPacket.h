#pragma once

#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

class CreatePhotoPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::CreatePhoto;

    CreatePhotoPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "CreatePhotoPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mId = 0;
    std::string mPhotoName;
    std::string mPhotoItemName;
};
