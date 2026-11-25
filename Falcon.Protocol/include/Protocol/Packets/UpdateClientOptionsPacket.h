#pragma once

#include "Protocol/Packet.h"

enum class GraphicsMode : unsigned char {
    Simple = 0,
    Fancy = 1,
    Advanced = 2,
    RayTraced = 3
};

class UpdateClientOptionsPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::UpdateClientOptions;

    UpdateClientOptionsPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "UpdateClientOptionsPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    bool mHasGraphicsMode = false;
    GraphicsMode mGraphicsMode = GraphicsMode::Simple;
    bool mHasFilterProfanityChange = false;
    bool mFilterProfanityChange = false;
};
