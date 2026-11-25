#pragma once

#include "Protocol/Packet.h"

#include <cstdint>

class MoveActorDeltaPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::MoveActorDelta;

    enum Flag : uint32_t {
        HasX = 1 << 0,
        HasY = 1 << 1,
        HasZ = 1 << 2,
        HasPitch = 1 << 3,
        HasYaw = 1 << 4,
        HasHeadYaw = 1 << 5,
    };

    MoveActorDeltaPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "MoveActorDeltaPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint64_t mRuntimeActorId = 0;
    float mX = 0.0f;
    float mY = 0.0f;
    float mZ = 0.0f;
    float mPitch = 0.0f;
    float mYaw = 0.0f;
    float mHeadYaw = 0.0f;
    bool mHasX = false;
    bool mHasY = false;
    bool mHasZ = false;
    bool mHasPitch = false;
    bool mHasYaw = false;
    bool mHasHeadYaw = false;
    bool mOnGround = false;
    bool mForceMove = false;
    bool mForceMoveLocalActor = false;
    bool mForceCompletion = false;
    uint64_t mTicks = 0;
};
