#pragma once

#include "Core/NBT/Tag.h"
#include "Protocol/Packet.h"

#include <cstdint>
#include <string>
#include <vector>

struct ItemComponentEntry {
    std::string mIdentifier;
    int16_t mRuntimeId = 0;
    bool mComponentBased = false;
    int32_t mItemVersion = 0;
    Tag mComponentData;
};

class ItemRegistryPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ItemRegistry;

    ItemRegistryPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ItemRegistryPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<ItemComponentEntry> mEntries;
};
