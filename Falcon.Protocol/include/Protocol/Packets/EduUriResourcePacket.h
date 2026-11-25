#pragma once

#include "Protocol/Packet.h"

#include <string>

class EduUriResourcePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::EduUriResource;

    EduUriResourcePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "EduUriResourcePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mButtonName;
    std::string mLinkUri;
};
