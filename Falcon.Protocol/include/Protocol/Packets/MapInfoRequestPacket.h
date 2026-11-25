#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/MapPixel.h"

#include <cstdint>
#include <vector>

class MapInfoRequestPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::MapInfoRequest;

    MapInfoRequestPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "MapInfoRequestPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mUniqueMapId = 0;
    std::vector<MapPixel> mPixels;
};
