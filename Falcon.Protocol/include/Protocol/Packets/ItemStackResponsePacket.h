#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/ItemStackResponseContainer.h"

#include <cstdint>
#include <vector>

class ItemStackResponseEntry {
public:
    int32_t mResult = 0;
    int32_t mRequestId = 0;
    std::vector<ItemStackResponseContainer> mContainers;
};

class ItemStackResponsePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ItemStackResponse;

    ItemStackResponsePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ItemStackResponsePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<ItemStackResponseEntry> mEntries;
};
