#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/PacketViolationData.h"

#include <cstdint>
#include <string>

class PacketViolationWarningPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::PacketViolationWarning;

    PacketViolationWarningPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "PacketViolationWarningPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    PacketViolationType mType = PacketViolationType::Unknown;
    PacketViolationSeverity mSeverity = PacketViolationSeverity::Unknown;
    int32_t mPacketCauseId = 0;
    std::string mContext;
};
