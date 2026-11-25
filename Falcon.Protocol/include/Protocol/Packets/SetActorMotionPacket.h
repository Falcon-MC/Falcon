#pragma once

#include "Protocol/Packet.h"

class SetActorMotionPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::SetActorMotion;

    SetActorMotionPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "SetActorMotionPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint64_t mRuntimeActorId;
    Vector3f mMotion;
    uint64_t mTick;
};
