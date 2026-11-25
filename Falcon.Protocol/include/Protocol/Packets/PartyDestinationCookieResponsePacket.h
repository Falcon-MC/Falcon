#pragma once

#include "Protocol/Packet.h"

#include <string>

class PartyDestinationCookieResponsePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::PartyDestinationCookieResponse;

    PartyDestinationCookieResponsePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "PartyDestinationCookieResponsePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mCookie;
    bool mAccepted = false;
};
