#pragma once

#include "Protocol/Packet.h"

class ServerSettingsResponsePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ServerSettingsResponse;

    ServerSettingsResponsePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ServerSettingsResponsePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint32_t mFormId;
    std::string mFormData;
};
