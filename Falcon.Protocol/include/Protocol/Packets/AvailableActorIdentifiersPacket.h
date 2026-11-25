#pragma once

#include "Core/NBT/Tag.h"
#include "Protocol/Packet.h"

class AvailableActorIdentifiersPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::AvailableActorIdentifiers;

    AvailableActorIdentifiersPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "AvailableActorIdentifiersPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Tag mIdentifiers;
};
