#include "Protocol/Packets/CodeBuilderSourcePacket.h"

#include "Protocol/NetworkPacketHandler.h"

CodeBuilderSourcePacket::CodeBuilderSourcePacket() = default;

void CodeBuilderSourcePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte((unsigned char) mOperation);
    stream.putByte((unsigned char) mCategory);
    stream.putByte((unsigned char) mCodeStatus);
}

void CodeBuilderSourcePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mOperation = (CodeBuilderOperationType) stream.getByte();
    mCategory = (CodeBuilderCategoryType) stream.getByte();
    mCodeStatus = (CodeBuilderCodeStatus) stream.getByte();
}

void CodeBuilderSourcePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
