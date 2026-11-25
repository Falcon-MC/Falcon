#pragma once

#include "Protocol/Packet.h"

class PlayerToggleArmorVisibilityRequestPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::PlayerToggleArmorVisibilityRequest;

    PlayerToggleArmorVisibilityRequestPacket();

    MinecraftPacketIds getId() const override {
        return ID;
    }

    const char *getName() const override {
        return "PlayerToggleArmorVisibilityRequestPacket";
    }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    bool mHideArmor = false;
};
