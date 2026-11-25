#pragma once

#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

class PlayerStartItemCooldownPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::PlayerStartItemCooldown;

    PlayerStartItemCooldownPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "PlayerStartItemCooldownPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mItemCategory;
    int32_t mCooldownDuration = 0;
};
