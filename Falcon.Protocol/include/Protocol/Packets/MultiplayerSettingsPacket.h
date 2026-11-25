#pragma once

#include "Protocol/Packet.h"

class MultiplayerSettingsPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::MultiplayerSettings;

    enum class Mode : int {
        EnableMultiplayer,
        DisableMultiplayer,
        RefreshJoinCode
    };

    MultiplayerSettingsPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "MultiplayerSettingsPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Mode mMode;
};
