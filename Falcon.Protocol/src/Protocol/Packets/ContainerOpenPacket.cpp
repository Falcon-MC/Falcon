#include "Protocol/Packets/ContainerOpenPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ContainerOpenPacket::ContainerOpenPacket() = default;

void ContainerOpenPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte((unsigned char) mWindowId);
    stream.putByte((unsigned char) (int8_t) containerTypeToId(mType));
    stream.putBlockPosition(mBlockPosition);
    stream.putVarLong(mUniqueActorId);
}

void ContainerOpenPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mWindowId = (int8_t) stream.getByte();
    mType = containerTypeFromId((int32_t) stream.getSignedByte());
    mBlockPosition = stream.getBlockPosition();
    mUniqueActorId = stream.getVarLong();
}

void ContainerOpenPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
