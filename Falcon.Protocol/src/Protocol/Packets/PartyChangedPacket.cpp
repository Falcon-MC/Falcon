#include "Protocol/Packets/PartyChangedPacket.h"

#include "Protocol/NetworkPacketHandler.h"

PartyChangedPacket::PartyChangedPacket() = default;

void PartyChangedPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putOptionalPresent(mHasParty);
    if (mHasParty) {
        stream.putString(mPartyId);
        stream.putBool(mIsPartyLeader);
    }
}

void PartyChangedPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mHasParty = stream.getOptionalPresent();
    if (mHasParty) {
        mPartyId = stream.getString();
        mIsPartyLeader = stream.getBool();
    }
}

void PartyChangedPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
