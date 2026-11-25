#pragma once

#include "Protocol/Packet.h"

#include <cstdint>
#include <string>
#include <vector>

class AnimateEntityPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::AnimateEntity;

    AnimateEntityPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "AnimateEntityPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mAnimation;
    std::string mNextState;
    std::string mStopExpression;
    int32_t mStopExpressionVersion = 0;
    std::string mController;
    float mBlendOutTime = 0.0f;
    std::vector<uint64_t> mRuntimeActorIds;
};
