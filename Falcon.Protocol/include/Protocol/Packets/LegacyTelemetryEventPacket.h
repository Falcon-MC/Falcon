#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/EventData.h"

#include <cstdint>

class LegacyTelemetryEventPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::LegacyTelemetryEvent;

    LegacyTelemetryEventPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "LegacyTelemetryEventPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mUniqueActorId = 0;
    bool mUsePlayerId = false;
    EventData mEventData;
};
