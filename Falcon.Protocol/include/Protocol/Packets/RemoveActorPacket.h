#pragma once

#include "Protocol/Packet.h"

class RemoveActorPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::RemoveActor;

    RemoveActorPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "RemoveActorPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mUniqueActorId;
};
