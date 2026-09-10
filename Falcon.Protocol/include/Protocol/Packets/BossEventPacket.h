#pragma once

#include "Protocol/Packet.h"

class BossEventPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::BossEvent;

    enum class Action : int {
        Create,
        RegisterPlayer,
        Remove,
        UnregisterPlayer,
        UpdatePercentage,
        UpdateName,
        UpdateProperties,
        UpdateStyle,
        Query
    };

    BossEventPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "BossEventPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mBossUniqueActorId;
    Action mAction;
    std::string mTitle;
    std::string mFilteredTitle;
    float mHealthPercentage;
    int32_t mColor;
    int32_t mOverlay;
};
