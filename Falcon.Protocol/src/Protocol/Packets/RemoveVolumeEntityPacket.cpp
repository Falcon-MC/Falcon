#include "Protocol/Packets/RemoveVolumeEntityPacket.h"

#include "Protocol/NetworkPacketHandler.h"

RemoveVolumeEntityPacket::RemoveVolumeEntityPacket() = default;

void RemoveVolumeEntityPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarInt(mId);
    stream.putVarInt(mDimension);
}

void RemoveVolumeEntityPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mId = stream.getUnsignedVarInt();
    mDimension = stream.getVarInt();
}

void RemoveVolumeEntityPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
