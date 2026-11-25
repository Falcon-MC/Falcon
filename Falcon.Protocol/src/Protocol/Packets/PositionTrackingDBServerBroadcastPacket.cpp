#include "Protocol/Packets/PositionTrackingDBServerBroadcastPacket.h"

#include "Core/NBT/NbtIo.h"
#include "Protocol/NetworkPacketHandler.h"

PositionTrackingDBServerBroadcastPacket::PositionTrackingDBServerBroadcastPacket() = default;

void PositionTrackingDBServerBroadcastPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte((unsigned char) mAction);
    stream.putVarInt(mTrackingId);
    NbtIo::writeTag(stream, mTag, NbtVariant::Network);
}

void PositionTrackingDBServerBroadcastPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mAction = (PositionTrackingDBBroadcastAction) stream.getByte();
    mTrackingId = stream.getVarInt();
    mTag = NbtIo::readTag(stream, NbtVariant::Network);
}

void PositionTrackingDBServerBroadcastPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
