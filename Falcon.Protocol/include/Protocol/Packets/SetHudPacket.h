#pragma once

#include "Protocol/Packet.h"

#include <cstdint>
#include <vector>

class SetHudPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::SetHud;

    enum class HudVisibility {
        Hide,
        Reset
    };

    SetHudPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "SetHudPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<int32_t> mElements;
    HudVisibility mVisibility = HudVisibility::Hide;
};
