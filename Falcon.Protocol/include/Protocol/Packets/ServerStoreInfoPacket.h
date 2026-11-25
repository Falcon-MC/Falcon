#pragma once

#include "Protocol/Packet.h"

#include <string>

class ServerStoreInfoPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ServerStoreInfo;

    ServerStoreInfoPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ServerStoreInfoPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    bool mHasStore = false;
    std::string mStoreId;
    std::string mStoreName;
};
