#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/ItemStack.h"

#include <cstdint>
#include <string>
#include <vector>

struct CreativeItemGroup {
    uint8_t mCategory = 0;
    std::string mName;
    ItemStack mIcon;
};

struct CreativeItemData {
    int32_t mNetId = 0;
    ItemStack mItem;
    int32_t mGroupIndex = 0;
};

class CreativeContentPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::CreativeContent;

    CreativeContentPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "CreativeContentPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<CreativeItemGroup> mGroups;
    std::vector<CreativeItemData> mItems;
};
