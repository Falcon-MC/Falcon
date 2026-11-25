#pragma once

#include "Protocol/Packet.h"

#include <cstdint>

class PlayerHotbarPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::PlayerHotbar;

    PlayerHotbarPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "PlayerHotbarPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int32_t mSelectedHotbarSlot = 0;
    int32_t mContainerId = 0;
    bool mSelectHotbarSlot = false;
};
