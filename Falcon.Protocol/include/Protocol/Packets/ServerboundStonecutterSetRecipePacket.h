#pragma once

#include "Protocol/Packet.h"

#include <cstdint>

class ServerboundStonecutterSetRecipePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ServerboundStonecutterSetRecipe;

    ServerboundStonecutterSetRecipePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ServerboundStonecutterSetRecipePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint8_t mContainerId = 0;
    int32_t mRecipeIndex = 0;
};
