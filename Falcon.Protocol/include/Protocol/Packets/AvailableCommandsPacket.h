#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/CommandData.h"

#include <string>
#include <vector>

class AvailableCommandsPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::AvailableCommands;

    AvailableCommandsPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "AvailableCommandsPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<CommandData> mCommands;
};
