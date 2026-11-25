#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/ItemStack.h"

#include <cstdint>
#include <string>
#include <vector>

// opens a screen onto a script owned container with no block or entity behind it
class VirtualContainerOpenPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::VirtualContainerOpen;

    VirtualContainerOpenPacket();

    MinecraftPacketIds getId() const override {
        return ID;
    }

    const char *getName() const override {
        return "VirtualContainerOpenPacket";
    }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint8_t mContainerId = 0;
    uint64_t mSessionId = 1;
    std::string mTitle;

    // the slot count is the container size
    std::vector<ItemStack> mSlots;
};
