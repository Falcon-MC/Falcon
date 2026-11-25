#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/ItemUseType.h"

#include <cstdint>

class CompletedUsingItemPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::CompletedUsingItem;

    CompletedUsingItemPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "CompletedUsingItemPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int32_t mItemId = 0;
    ItemUseType mType = ItemUseType::Unknown;
};
