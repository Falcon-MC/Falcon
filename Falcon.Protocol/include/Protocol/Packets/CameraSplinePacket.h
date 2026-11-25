#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/CameraTypes.h"

#include <vector>

class CameraSplinePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::CameraSpline;

    CameraSplinePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "CameraSplinePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<CameraSplineDefinition> mSplines;
};
