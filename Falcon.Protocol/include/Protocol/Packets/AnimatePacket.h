#pragma once

#include "Protocol/Packet.h"

#include <cstdint>
#include <optional>

class AnimatePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::Animate;

    enum class Action : int {
        NoAction,
        SwingArm,
        WakeUp,
        CriticalHit,
        MagicCriticalHit,
        RowRight,
        RowLeft
    };

    AnimatePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "AnimatePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Action mAction;
    uint64_t mRuntimeActorId;
    float mData;
    std::string mSwingSource;
    std::optional<uint8_t> mHand;
};
