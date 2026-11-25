#pragma once

#include "Core/Math/Vector3i.h"
#include "Protocol/Packet.h"
#include "Protocol/Types/LabTableReactionType.h"
#include "Protocol/Types/LabTableType.h"

class LabTablePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::LabTable;

    LabTablePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "LabTablePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    LabTableType mType = LabTableType::StartCombine;
    Vector3i mPosition;
    LabTableReactionType mReactionType = LabTableReactionType::None;
};
