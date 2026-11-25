#pragma once

#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

class ClientboundDataDrivenUIShowScreenPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ClientboundDataDrivenUIShowScreen;

    ClientboundDataDrivenUIShowScreenPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ClientboundDataDrivenUIShowScreenPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mScreenId;
    int32_t mFormId = 0;

    bool mHasDataInstanceId = false;
    int32_t mDataInstanceId = 0;
};
