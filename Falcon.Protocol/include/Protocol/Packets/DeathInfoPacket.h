#pragma once

#include "Protocol/Packet.h"

#include <string>
#include <vector>

class DeathInfoPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::DeathInfo;

    DeathInfoPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "DeathInfoPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mCauseAttackName;
    std::vector<std::string> mMessageList;
};
