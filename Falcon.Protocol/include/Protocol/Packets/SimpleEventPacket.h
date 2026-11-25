#pragma once

#include "Protocol/Packet.h"

class SimpleEventPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::SimpleEvent;

    SimpleEventPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "SimpleEventPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint16_t mEvent;
};
