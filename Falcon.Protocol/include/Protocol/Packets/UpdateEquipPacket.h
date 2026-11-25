#pragma once

#include "Core/NBT/Tag.h"
#include "Protocol/Packet.h"

#include <cstdint>

class UpdateEquipPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::UpdateEquip;

    UpdateEquipPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "UpdateEquipPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int32_t mWindowId = 0;
    int32_t mWindowType = 0;
    int32_t mSize = 0;
    int64_t mUniqueActorId = 0;
    Tag mTag;
};
