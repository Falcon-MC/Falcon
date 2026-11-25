#pragma once

#include "Protocol/Packet.h"

class UpdateAdventureSettingsPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::UpdateAdventureSettings;

    UpdateAdventureSettingsPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "UpdateAdventureSettingsPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    bool mNoPvM = false;
    bool mNoMvP = false;
    bool mImmutableWorld = false;
    bool mShowNameTags = false;
    bool mAutoJump = false;
};
