#pragma once

#include "Core/Utility/UUID.h"
#include "Protocol/Packet.h"
#include "Protocol/Types/CraftingType.h"
#include "Protocol/Types/ItemStack.h"

#include <cstdint>
#include <vector>

class CraftingEventPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::CraftingEvent;

    CraftingEventPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "CraftingEventPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int8_t mContainerId = 0;
    CraftingType mType = CraftingType::Inventory;
    Uuid mUuid;
    std::vector<ItemStack> mInputs;
    std::vector<ItemStack> mOutputs;
};
