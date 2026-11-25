#pragma once

#include "Core/Math/Vector3f.h"
#include "Protocol/Packet.h"

#include <cstdint>
#include <vector>

class ClientMovementPredictionSyncPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ClientMovementPredictionSync;

    ClientMovementPredictionSyncPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ClientMovementPredictionSyncPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint64_t mRuntimeActorId = 0;
    std::vector<bool> mFlags;
    Vector3f mBoundingBox;
    float mSpeed = 0.0f;
    float mUnderwaterSpeed = 0.0f;
    float mLavaSpeed = 0.0f;
    float mJumpStrength = 0.0f;
    float mHealth = 0.0f;
    float mHunger = 0.0f;
    float mUnknown1 = 0.0f;
    float mUnknown2 = 0.0f;
    float mUnknown3 = 0.0f;
    bool mFlying = false;
};
