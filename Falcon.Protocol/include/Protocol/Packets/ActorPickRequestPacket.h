#pragma once

#include "Protocol/Packet.h"

class ActorPickRequestPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ActorPickRequest;

    ActorPickRequestPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ActorPickRequestPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint64_t mRuntimeActorId;
    int32_t mHotbarSlot;
    bool mWithData;
};
