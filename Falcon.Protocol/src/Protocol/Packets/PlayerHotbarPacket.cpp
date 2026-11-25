#include "Protocol/Packets/PlayerHotbarPacket.h"

#include "Protocol/NetworkPacketHandler.h"

PlayerHotbarPacket::PlayerHotbarPacket() = default;

void PlayerHotbarPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarInt((uint32_t) mSelectedHotbarSlot);
    stream.putByte((unsigned char) mContainerId);
    stream.putBool(mSelectHotbarSlot);
}

void PlayerHotbarPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mSelectedHotbarSlot = (int32_t) stream.getUnsignedVarInt();
    mContainerId = stream.getByte();
    mSelectHotbarSlot = stream.getBool();
}

void PlayerHotbarPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
