#include "Protocol/Packets/LabTablePacket.h"

#include "Protocol/NetworkPacketHandler.h"

LabTablePacket::LabTablePacket() = default;

void LabTablePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte((unsigned char) labTableTypeToId(mType));
    stream.putVector3i(mPosition);
    stream.putByte((unsigned char) labTableReactionTypeToId(mReactionType));
}

void LabTablePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mType = labTableTypeFromId(stream.getByte());
    mPosition = stream.getVector3i();
    mReactionType = labTableReactionTypeFromId(stream.getByte());
}

void LabTablePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
