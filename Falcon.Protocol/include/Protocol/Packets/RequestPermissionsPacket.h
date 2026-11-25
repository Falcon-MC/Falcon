#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/StartGameTypes.h"

#include <cstdint>

class RequestPermissionsPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::RequestPermissions;

    RequestPermissionsPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "RequestPermissionsPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mUniqueActorId = 0;
    PlayerPermission mPermissions = PlayerPermission::Visitor;
    int32_t mCustomPermissions = 0;
};
