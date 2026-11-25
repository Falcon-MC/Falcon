#pragma once

#include "Core/Math/Vector3i.h"
#include "Protocol/Packet.h"
#include "Protocol/Packets/UpdateBlockPacket.h"

#include <cstdint>

enum class BlockSyncType {
    None,
    Create,
    Destroy,
};

class UpdateBlockSyncedPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::UpdateBlockSynced;

    UpdateBlockSyncedPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "UpdateBlockSyncedPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Vector3i mBlockPosition;
    uint32_t mRuntimeId;
    uint32_t mFlags;
    uint32_t mDataLayer;
    uint64_t mRuntimeActorId = 0;
    BlockSyncType mEntityBlockSyncType = BlockSyncType::None;
};
