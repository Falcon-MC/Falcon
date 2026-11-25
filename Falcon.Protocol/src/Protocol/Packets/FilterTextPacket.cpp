#include "Protocol/Packets/FilterTextPacket.h"

#include "Protocol/NetworkPacketHandler.h"

FilterTextPacket::FilterTextPacket() = default;

void FilterTextPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mText);
    stream.putBool(mFromServer);
}

void FilterTextPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mText = stream.getString();
    mFromServer = stream.getBool();
}

void FilterTextPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
