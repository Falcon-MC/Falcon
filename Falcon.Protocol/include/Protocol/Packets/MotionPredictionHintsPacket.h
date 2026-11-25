#pragma once

#include "Core/Math/Vector3f.h"
#include "Protocol/Packet.h"

#include <cstdint>

class MotionPredictionHintsPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::MotionPredictionHints;

    MotionPredictionHintsPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "MotionPredictionHintsPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint64_t mRuntimeActorId = 0;
    Vector3f mMotion;
    bool mOnGround = false;
};
