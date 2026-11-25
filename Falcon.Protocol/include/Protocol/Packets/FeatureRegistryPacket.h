#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/FeatureDefinition.h"

#include <vector>

class FeatureRegistryPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::FeatureRegistry;

    FeatureRegistryPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "FeatureRegistryPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<FeatureDefinition> mFeatures;
};
