#pragma once

#include "Protocol/Packet.h"

class RespawnPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::Respawn;

    enum class State : int {
        ServerSearching,
        ServerReady,
        ClientReady
    };

    RespawnPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "RespawnPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Vector3f mPosition;
    State mState;
    uint64_t mRuntimeActorId;
};
