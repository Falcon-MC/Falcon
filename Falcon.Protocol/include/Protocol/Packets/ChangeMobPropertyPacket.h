#pragma once

#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

class ChangeMobPropertyPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ChangeMobProperty;

    ChangeMobPropertyPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ChangeMobPropertyPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mUniqueActorId = 0;
    std::string mProperty;
    bool mBoolValue = false;
    std::string mStringValue;
    int32_t mIntValue = 0;
    float mFloatValue = 0.0f;
};
