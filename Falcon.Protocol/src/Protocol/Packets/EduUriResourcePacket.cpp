#include "Protocol/Packets/EduUriResourcePacket.h"

#include "Protocol/NetworkPacketHandler.h"

EduUriResourcePacket::EduUriResourcePacket() = default;

void EduUriResourcePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mButtonName);
    stream.putString(mLinkUri);
}

void EduUriResourcePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mButtonName = stream.getString();
    mLinkUri = stream.getString();
}

void EduUriResourcePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
