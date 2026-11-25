#include "Protocol/Packets/PartyDestinationCookieResponsePacket.h"

#include "Protocol/NetworkPacketHandler.h"

PartyDestinationCookieResponsePacket::PartyDestinationCookieResponsePacket() = default;

void PartyDestinationCookieResponsePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mCookie);
    stream.putBool(mAccepted);
}

void PartyDestinationCookieResponsePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mCookie = stream.getString();
    mAccepted = stream.getBool();
}

void PartyDestinationCookieResponsePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
