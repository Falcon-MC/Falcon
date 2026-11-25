#pragma once

#include "Protocol/Packet.h"

#include <cstdint>

class EntityFallPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::EntityFall;

    EntityFallPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "EntityFallPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint64_t mRuntimeActorId = 0;
    float mFallDistance = 0.0f;
    bool mInVoid = false;
};
