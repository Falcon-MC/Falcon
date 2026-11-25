#pragma once

#include "Protocol/Packet.h"

class MapCreateLockedCopyPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::MapCreateLockedCopy;

    MapCreateLockedCopyPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "MapCreateLockedCopyPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mOriginalMapId;
    int64_t mNewMapId;
};
