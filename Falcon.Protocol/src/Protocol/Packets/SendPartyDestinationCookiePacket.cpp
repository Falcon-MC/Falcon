#include "Protocol/Packets/SendPartyDestinationCookiePacket.h"

#include "Protocol/NetworkPacketHandler.h"

namespace {

    std::string intentToName(PartyDestinationIntent intent) {
        switch (intent) {
            case PartyDestinationIntent::Notify:
                return "notify";
            case PartyDestinationIntent::OptIn:
                return "optin";
            case PartyDestinationIntent::OptOut:
                return "optout";
            default:
                return "notify";
        }
    }

    PartyDestinationIntent intentFromName(const std::string &name) {
        if (name == "optin")
            return PartyDestinationIntent::OptIn;
        if (name == "optout")
            return PartyDestinationIntent::OptOut;
        return PartyDestinationIntent::Notify;
    }

}

SendPartyDestinationCookiePacket::SendPartyDestinationCookiePacket() = default;

void SendPartyDestinationCookiePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mCookie);
    stream.putString(intentToName(mIntent));
    stream.putString(mDestinationName);
}

void SendPartyDestinationCookiePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mCookie = stream.getString();
    mIntent = intentFromName(stream.getString());
    mDestinationName = stream.getString();
}

void SendPartyDestinationCookiePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
