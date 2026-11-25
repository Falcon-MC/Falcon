#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/CameraTypes.h"

class ClientboundControlSchemeSetPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ClientboundControlSchemeSet;

    ClientboundControlSchemeSetPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ClientboundControlSchemeSetPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    ControlScheme mScheme = ControlScheme::LockedPlayerRelativeStrafe;
};
