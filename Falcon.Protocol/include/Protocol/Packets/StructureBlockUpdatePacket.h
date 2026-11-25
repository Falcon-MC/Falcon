#pragma once

#include "Core/Math/Vector3i.h"
#include "Protocol/Packet.h"
#include "Protocol/Types/StructureData.h"

class StructureBlockUpdatePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::StructureBlockUpdate;

    StructureBlockUpdatePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "StructureBlockUpdatePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Vector3i mBlockPosition;
    StructureEditorData mEditorData;
    bool mPowered = false;
    bool mWaterlogged = false;
};
