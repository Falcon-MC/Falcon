#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/PlayerArmorDamageFlag.h"

#include <array>
#include <cstdint>
#include <vector>

class PlayerArmorDamagePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::PlayerArmorDamage;

    PlayerArmorDamagePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "PlayerArmorDamagePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<PlayerArmorDamageFlag> mFlags;
    std::array<int32_t, 5> mDamage = {0, 0, 0, 0, 0};
};
