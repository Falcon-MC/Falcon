#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/FullContainerName.h"

#include <vector>

class ContainerRegistryCleanupPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ContainerRegistryCleanup;

    ContainerRegistryCleanupPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ContainerRegistryCleanupPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<FullContainerName> mContainers;
};
