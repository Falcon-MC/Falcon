#include "Protocol/Packets/PhotoTransferPacket.h"

#include "Protocol/NetworkPacketHandler.h"

PhotoTransferPacket::PhotoTransferPacket() = default;

void PhotoTransferPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mName);
    stream.putByteArray(mData);
    stream.putString(mBookId);
    stream.putByte((unsigned char) mPhotoType);
    stream.putByte((unsigned char) mSourceType);
    stream.putLLong((uint64_t) mOwnerId);
    stream.putString(mNewPhotoName);
}

void PhotoTransferPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mName = stream.getString();
    mData = stream.getByteArray();
    mBookId = stream.getString();
    mPhotoType = (PhotoType) stream.getByte();
    mSourceType = (PhotoType) stream.getByte();
    mOwnerId = (int64_t) stream.getLLong();
    mNewPhotoName = stream.getString();
}

void PhotoTransferPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
