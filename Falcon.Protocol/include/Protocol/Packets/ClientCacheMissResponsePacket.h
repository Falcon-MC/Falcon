#pragma once

#include "Protocol/Packet.h"

#include <string>
#include <utility>
#include <vector>

class ClientCacheMissResponsePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ClientCacheMissResponse;

    ClientCacheMissResponsePacket() = default;

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ClientCacheMissResponsePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    // blob id to blob payload, kept in insertion order like the vanilla map
    std::vector<std::pair<uint64_t, std::string> > mBlobs;
};
