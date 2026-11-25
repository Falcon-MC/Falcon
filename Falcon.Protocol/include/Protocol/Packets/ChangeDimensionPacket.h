#pragma once

#include "Protocol/Packet.h"

class ChangeDimensionPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ChangeDimension;

    ChangeDimensionPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ChangeDimensionPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int32_t mDimension;
    Vector3f mPosition;
    bool mRespawn;
    bool mHasLoadingScreenId;
    int32_t mLoadingScreenId;
};
