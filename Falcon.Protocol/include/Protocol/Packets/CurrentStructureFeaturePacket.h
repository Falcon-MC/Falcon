#pragma once

#include "Protocol/Packet.h"

#include <string>

class CurrentStructureFeaturePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::CurrentStructureFeature;

    CurrentStructureFeaturePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "CurrentStructureFeaturePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mCurrentStructureFeature;
};
