#pragma once

#include "Protocol/Packet.h"

class SetDifficultyPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::SetDifficulty;

    SetDifficultyPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "SetDifficultyPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint32_t mDifficulty;
};
