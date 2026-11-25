#pragma once

#include "Protocol/Packet.h"

class BlockPickRequestPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::BlockPickRequest;

    BlockPickRequestPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "BlockPickRequestPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Vector3i mBlockPosition;
    bool mAddUserData;
    int32_t mHotbarSlot;
};
