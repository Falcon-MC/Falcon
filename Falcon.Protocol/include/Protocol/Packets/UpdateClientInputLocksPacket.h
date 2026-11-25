#pragma once

#include "Protocol/Packet.h"

#include <cstdint>

class UpdateClientInputLocksPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::UpdateClientInputLocks;

    UpdateClientInputLocksPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "UpdateClientInputLocksPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int32_t mLockComponentData = 0;
};
