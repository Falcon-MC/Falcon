#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/InventoryLayout.h"
#include "Protocol/Types/InventoryTabLeft.h"
#include "Protocol/Types/InventoryTabRight.h"

class SetPlayerInventoryOptionsPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::SetPlayerInventoryOptions;

    SetPlayerInventoryOptionsPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "SetPlayerInventoryOptionsPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    InventoryTabLeft mLeftTab = InventoryTabLeft::None;
    InventoryTabRight mRightTab = InventoryTabRight::None;
    bool mFiltering = false;
    InventoryLayout mLayout = InventoryLayout::None;
    InventoryLayout mCraftingLayout = InventoryLayout::None;
};
