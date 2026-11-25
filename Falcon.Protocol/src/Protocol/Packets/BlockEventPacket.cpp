#include "Protocol/Packets/BlockEventPacket.h"

#include "Protocol/NetworkPacketHandler.h"

BlockEventPacket::BlockEventPacket() = default;

void BlockEventPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putBlockPosition(mBlockPosition);
    stream.putVarInt(mEventType);
    stream.putVarInt(mEventData);
}

void BlockEventPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mBlockPosition = stream.getBlockPosition();
    mEventType = stream.getVarInt();
    mEventData = stream.getVarInt();
}

void BlockEventPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
