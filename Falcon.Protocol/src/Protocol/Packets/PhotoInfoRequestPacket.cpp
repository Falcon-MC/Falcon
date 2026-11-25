#include "Protocol/Packets/PhotoInfoRequestPacket.h"

#include "Protocol/NetworkPacketHandler.h"

PhotoInfoRequestPacket::PhotoInfoRequestPacket() = default;

void PhotoInfoRequestPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarLong(mPhotoId);
}

void PhotoInfoRequestPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mPhotoId = stream.getVarLong();
}

void PhotoInfoRequestPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
