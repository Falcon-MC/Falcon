#pragma once

#include "Core/Math/Vector3i.h"
#include "Protocol/Packet.h"

#include <cstdint>

class PlayerToggleCrafterSlotRequestPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::PlayerToggleCrafterSlotRequest;

    PlayerToggleCrafterSlotRequestPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "PlayerToggleCrafterSlotRequestPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Vector3i mBlockPosition;
    int8_t mSlot = 0;
    bool mDisabled = false;
};
