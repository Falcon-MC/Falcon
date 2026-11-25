#pragma once

#include "Protocol/Packet.h"

#include <cstdint>

enum class PlayerUpdateEntityOverrideType : int32_t {
    ClearOverrides = 0,
    RemoveOverride = 1,
    SetIntOverride = 2,
    SetFloatOverride = 3
};

class PlayerUpdateEntityOverridesPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::PlayerUpdateEntityOverrides;

    PlayerUpdateEntityOverridesPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "PlayerUpdateEntityOverridesPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mActorUniqueId = 0;
    int32_t mPropertyIndex = 0;
    PlayerUpdateEntityOverrideType mUpdateType = PlayerUpdateEntityOverrideType::ClearOverrides;
    int32_t mIntValue = 0;
    float mFloatValue = 0.0f;
};
