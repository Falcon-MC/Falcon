#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/PassengerOfBlockData.h"

#include <cstdint>

class SetPassengerOfBlockPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::SetPassengerOfBlock;

    SetPassengerOfBlockPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "SetPassengerOfBlockPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mPassengerUniqueId = 0;
    bool mHasData = false;
    PassengerOfBlockData mData;
};
