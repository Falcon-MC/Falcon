#pragma once

#include "Protocol/Packet.h"

#include <cstdint>

class RemoveVolumeEntityPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::RemoveVolumeEntity;

    RemoveVolumeEntityPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "RemoveVolumeEntityPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint32_t mId = 0;
    int32_t mDimension = 0;
};
