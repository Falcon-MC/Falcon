#pragma once

#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

class GuiDataPickItemPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::GuiDataPickItem;

    GuiDataPickItemPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "GuiDataPickItemPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mDescription;
    std::string mItemEffects;
    int32_t mHotbarSlot = 0;
};
