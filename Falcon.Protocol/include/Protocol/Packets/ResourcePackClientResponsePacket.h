#pragma once

#include "Protocol/Packet.h"

#include <string>
#include <vector>

class ResourcePackClientResponsePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ResourcePackClientResponse;

    enum class Status : int {
        None = 0,
        Refused = 1,
        SendPacks = 2,
        HaveAllPacks = 3,
        Completed = 4
    };

    ResourcePackClientResponsePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ResourcePackClientResponsePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Status mStatus;
    std::vector<std::string> mPackIds;
};
