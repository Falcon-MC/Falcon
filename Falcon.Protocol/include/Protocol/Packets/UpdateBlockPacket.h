#pragma once

#include "Core/Math/Vector3i.h"
#include "Protocol/Packet.h"

class UpdateBlockPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::UpdateBlock;

    enum Flag : uint32_t {
        Neighbors = 1 << 0,
        Network = 1 << 1,
        NoGraphic = 1 << 2,
        Unused = 1 << 3,
        Priority = 1 << 4,

        All = Neighbors | Network,
        AllPriority = Neighbors | Network | Priority
    };

    UpdateBlockPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "UpdateBlockPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Vector3i mBlockPosition;
    uint32_t mRuntimeId;
    uint32_t mFlags;
    uint32_t mDataLayer;
};
