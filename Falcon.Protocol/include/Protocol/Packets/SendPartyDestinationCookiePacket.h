#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/PartyDestinationIntent.h"

#include <string>

class SendPartyDestinationCookiePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::SendPartyDestinationCookie;

    SendPartyDestinationCookiePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "SendPartyDestinationCookiePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mCookie;
    PartyDestinationIntent mIntent = PartyDestinationIntent::Notify;
    std::string mDestinationName;
};
