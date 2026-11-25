#include "Protocol/Packets/ContainerSetDataPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ContainerSetDataPacket::ContainerSetDataPacket() = default;

void ContainerSetDataPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte((unsigned char) mWindowId);
    stream.putVarInt(mProperty);
    stream.putVarInt(mValue);
}

void ContainerSetDataPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mWindowId = (int8_t) stream.getByte();
    mProperty = stream.getVarInt();
    mValue = stream.getVarInt();
}

void ContainerSetDataPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
