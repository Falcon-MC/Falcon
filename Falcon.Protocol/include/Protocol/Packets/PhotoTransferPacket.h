#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/PhotoType.h"

#include <cstdint>
#include <string>

class PhotoTransferPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::PhotoTransfer;

    PhotoTransferPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "PhotoTransferPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mName;
    std::string mData;
    std::string mBookId;
    PhotoType mPhotoType = PhotoType::Portfolio;
    PhotoType mSourceType = PhotoType::Portfolio;
    int64_t mOwnerId = 0;
    std::string mNewPhotoName;
};
