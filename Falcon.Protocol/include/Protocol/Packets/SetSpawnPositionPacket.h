#pragma once

#include "Protocol/Packet.h"

class SetSpawnPositionPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::SetSpawnPosition;

    enum class Type : int {
        PlayerSpawn,
        WorldSpawn
    };

    SetSpawnPositionPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "SetSpawnPositionPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Type mSpawnType;
    Vector3i mBlockPosition;
    int32_t mDimensionId;
    Vector3i mSpawnPosition;
};
