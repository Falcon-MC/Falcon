#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/FurnaceLayout.h"
#include "Protocol/Types/FurnaceTabLeft.h"
#include "Protocol/Types/FurnaceType.h"

class SetPlayerFurnaceOptionsPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::SetPlayerFurnaceOptions;

    SetPlayerFurnaceOptionsPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "SetPlayerFurnaceOptionsPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    FurnaceType mFurnaceType = FurnaceType::None;
    FurnaceTabLeft mLeftTab = FurnaceTabLeft::None;
    bool mFiltering = false;
    FurnaceLayout mLayout = FurnaceLayout::None;
};
