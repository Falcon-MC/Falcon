#pragma once

#include "Core/NBT/Tag.h"
#include "Protocol/Packet.h"

class EditorNetworkPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::EditorNetwork;

    EditorNetworkPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "EditorNetworkPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    bool mRouteToManager = false;
    Tag mPayload;
};
