#pragma once

#include "Core/Math/Vector3i.h"
#include "Core/NBT/Tag.h"
#include "Protocol/Packet.h"

class BlockActorDataPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::BlockActorData;

    BlockActorDataPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "BlockActorDataPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Vector3i mBlockPosition;
    Tag mData;
};
