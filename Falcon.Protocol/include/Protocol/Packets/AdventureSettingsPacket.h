#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/AdventureSettingData.h"

#include <cstdint>
#include <vector>

class AdventureSettingsPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::AdventureSettings;

    AdventureSettingsPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "AdventureSettingsPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<AdventureSetting> mSettings;
    CommandPermission mCommandPermission = CommandPermission::Any;
    PlayerPermission mPlayerPermission = PlayerPermission::Visitor;
    int64_t mUniqueActorId = 0;
};
