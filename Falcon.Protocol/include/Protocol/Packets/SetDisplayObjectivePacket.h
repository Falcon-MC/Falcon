#pragma once

#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

class SetDisplayObjectivePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::SetDisplayObjective;

    SetDisplayObjectivePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "SetDisplayObjectivePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mDisplaySlot;
    std::string mObjectiveId;
    std::string mDisplayName;
    std::string mCriteria;
    int32_t mSortOrder = 0;
};
