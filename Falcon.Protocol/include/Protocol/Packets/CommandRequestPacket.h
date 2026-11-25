#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/CommandOriginData.h"

#include <cstdint>
#include <string>

class CommandRequestPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::CommandRequest;

    CommandRequestPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "CommandRequestPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mCommand;
    CommandOriginData mOrigin;
    bool mInternal = false;
    int32_t mVersion = 48;
};
