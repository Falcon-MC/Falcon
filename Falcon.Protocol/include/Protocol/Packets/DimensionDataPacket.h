#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/DimensionDefinition.h"

#include <vector>

class DimensionDataPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::DimensionData;

    DimensionDataPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "DimensionDataPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<DimensionDefinition> mDefinitions;
};
