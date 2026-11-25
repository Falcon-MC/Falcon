#pragma once

#include "Protocol/Packet.h"

#include <cstdint>

class AgentAnimationPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::AgentAnimation;

    AgentAnimationPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "AgentAnimationPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int8_t mAnimation = 0;
    int64_t mRuntimeActorId = 0;
};
