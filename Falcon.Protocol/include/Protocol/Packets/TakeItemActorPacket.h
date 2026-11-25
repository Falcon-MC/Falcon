#pragma once

#include "Protocol/Packet.h"

class TakeItemActorPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::TakeItemActor;

    TakeItemActorPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "TakeItemActorPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint64_t mItemRuntimeActorId;
    uint64_t mRuntimeActorId;
};
