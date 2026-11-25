#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/SerializableVoxelShape.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class VoxelShapesPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::VoxelShapes;

    VoxelShapesPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "VoxelShapesPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<SerializableVoxelShape> mShapes;
    std::vector<std::pair<std::string, uint16_t>> mNameMap;
    uint16_t mCustomShapeCount = 0;
};
