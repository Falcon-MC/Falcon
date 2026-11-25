#pragma once

#include "Core/Math/Vector3f.h"
#include "Protocol/Packet.h"



#include <cstdint>

enum class PredictionType {
    Player,
    Vehicle,
};

class CorrectPlayerMovePredictionPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::CorrectPlayerMovePrediction;

    CorrectPlayerMovePredictionPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "CorrectPlayerMovePredictionPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    PredictionType mPredictionType = PredictionType::Player;
    Vector3f mPosition;
    Vector3f mDelta;
    Vector2f mVehicleRotation;
    bool mHasVehicleAngularVelocity = false;
    float mVehicleAngularVelocity = 0.0f;
    bool mOnGround = false;
    uint64_t mTick = 0;
};
