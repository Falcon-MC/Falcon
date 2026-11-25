#pragma once

#include "Protocol/Packet.h"

#include <cstdint>

class ContainerSetDataPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ContainerSetData;

    ContainerSetDataPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ContainerSetDataPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int8_t mWindowId = 0;
    int32_t mProperty = 0;
    int32_t mValue = 0;
};
