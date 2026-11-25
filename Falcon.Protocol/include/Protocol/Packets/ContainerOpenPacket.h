#pragma once

#include "Core/Math/Vector3i.h"
#include "Protocol/Packet.h"
#include "Protocol/Types/ContainerType.h"

#include <cstdint>

class ContainerOpenPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ContainerOpen;

    ContainerOpenPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ContainerOpenPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int8_t mWindowId = 0;
    ContainerType mType = ContainerType::None;
    Vector3i mBlockPosition;
    int64_t mUniqueActorId = -1;
};
