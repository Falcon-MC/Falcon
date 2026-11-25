#include "Protocol/Packets/PositionTrackingDBClientRequestPacket.h"

#include "Protocol/NetworkPacketHandler.h"

PositionTrackingDBClientRequestPacket::PositionTrackingDBClientRequestPacket() = default;

void PositionTrackingDBClientRequestPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte((unsigned char) mAction);
    stream.putVarInt(mTrackingId);
}

void PositionTrackingDBClientRequestPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mAction = (PositionTrackingDBRequestAction) stream.getByte();
    mTrackingId = stream.getVarInt();
}

void PositionTrackingDBClientRequestPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
