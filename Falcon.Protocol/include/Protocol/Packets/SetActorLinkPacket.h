#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/EntityLinkData.h"

class SetActorLinkPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::SetActorLink;

    SetActorLinkPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "SetActorLinkPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    EntityLinkData mActorLink;
};
