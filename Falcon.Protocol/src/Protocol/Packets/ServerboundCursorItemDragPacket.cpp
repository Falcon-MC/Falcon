#include "Protocol/Packets/ServerboundCursorItemDragPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ServerboundCursorItemDragPacket::ServerboundCursorItemDragPacket() = default;

void ServerboundCursorItemDragPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte((unsigned char) mState);
}

void ServerboundCursorItemDragPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mState = (State) stream.getByte();
}

void ServerboundCursorItemDragPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
