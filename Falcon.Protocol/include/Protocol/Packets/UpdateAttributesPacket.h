#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/AttributeData.h"

#include <cstdint>
#include <vector>

class UpdateAttributesPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::UpdateAttributes;

    UpdateAttributesPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "UpdateAttributesPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mRuntimeActorId = 0;
    std::vector<AttributeData> mAttributes;
    int64_t mTick = 0;
};
