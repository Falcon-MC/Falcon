#include "Protocol/Packets/CompletedUsingItemPacket.h"

#include "Protocol/NetworkPacketHandler.h"

CompletedUsingItemPacket::CompletedUsingItemPacket() = default;

void CompletedUsingItemPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putLShort((uint16_t) mItemId);
    stream.putLInt((uint32_t) (static_cast<int32_t>(mType) - 1));
}

void CompletedUsingItemPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mItemId = (int32_t) stream.getLShort();
    mType = static_cast<ItemUseType>(stream.getSignedLInt() + 1);
}

void CompletedUsingItemPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
