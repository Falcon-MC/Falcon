#include "Protocol/Packets/CreatePhotoPacket.h"

#include "Protocol/NetworkPacketHandler.h"

CreatePhotoPacket::CreatePhotoPacket() = default;

void CreatePhotoPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putLLong((uint64_t) mId);
    stream.putString(mPhotoName);
    stream.putString(mPhotoItemName);
}

void CreatePhotoPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mId = (int64_t) stream.getLLong();
    mPhotoName = stream.getString();
    mPhotoItemName = stream.getString();
}

void CreatePhotoPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
