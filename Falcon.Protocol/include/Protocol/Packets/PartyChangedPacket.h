#pragma once

#include "Protocol/Packet.h"

#include <string>

class PartyChangedPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::PartyChanged;

    PartyChangedPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "PartyChangedPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    bool mHasParty = false;
    std::string mPartyId;
    bool mIsPartyLeader = false;
};
