#pragma once

#include "Core/Math/Vector3i.h"
#include "Protocol/Packet.h"

#include <cstdint>

class BlockEventPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::BlockEvent;

    BlockEventPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "BlockEventPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Vector3i mBlockPosition;
    int32_t mEventType = 0;
    int32_t mEventData = 0;
};
