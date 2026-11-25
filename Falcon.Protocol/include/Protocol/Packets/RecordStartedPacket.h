#pragma once

#include "Core/Math/Vector3i.h"
#include "Protocol/Packet.h"

#include <cstdint>

class RecordStartedPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::RecordStarted;

    RecordStartedPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "RecordStartedPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Vector3i mBlockPosition;
    int64_t mServerSoundHandle = 0;
};
