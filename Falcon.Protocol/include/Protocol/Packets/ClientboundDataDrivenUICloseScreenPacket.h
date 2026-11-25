#pragma once

#include "Protocol/Packet.h"

#include <cstdint>

class ClientboundDataDrivenUICloseScreenPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ClientboundDataDrivenUICloseScreen;

    ClientboundDataDrivenUICloseScreenPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ClientboundDataDrivenUICloseScreenPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    bool mHasFormId = false;
    int32_t mFormId = 0;
};
