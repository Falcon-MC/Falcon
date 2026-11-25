#pragma once

#include "Protocol/Packet.h"

#include <cstdint>

enum class MovementEffectType : int32_t {
    Invalid = -1,
    GlideBoost = 0,
    DolphinBoost = 1,
    GeyserBoost = 2,
};

class MovementEffectPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::MovementEffect;

    MovementEffectPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "MovementEffectPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint64_t mActorRuntimeId = 0;
    MovementEffectType mEffectType = MovementEffectType::Invalid;
    uint32_t mDuration = 0;
    uint64_t mTick = 0;
};
