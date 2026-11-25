#include "Protocol/Packets/ContainerClosePacket.h"

#include "Protocol/NetworkPacketHandler.h"

ContainerClosePacket::ContainerClosePacket() = default;

void ContainerClosePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte((unsigned char) mWindowId);
    stream.putByte((unsigned char) (int8_t) containerTypeToId(mType));
    stream.putBool(mServerInitiated);
}

void ContainerClosePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mWindowId = (int8_t) stream.getByte();
    mType = containerTypeFromId((int32_t) stream.getSignedByte());
    mServerInitiated = stream.getBool();
}

void ContainerClosePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
