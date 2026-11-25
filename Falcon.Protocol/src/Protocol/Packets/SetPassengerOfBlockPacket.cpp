#include "Protocol/Packets/SetPassengerOfBlockPacket.h"

#include "Protocol/EntityCodec.h"
#include "Protocol/NetworkPacketHandler.h"

SetPassengerOfBlockPacket::SetPassengerOfBlockPacket() = default;

void SetPassengerOfBlockPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarLong(mPassengerUniqueId);
    EntityCodec::writePassengerOfBlock(stream, mHasData, mData);
}

void SetPassengerOfBlockPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mPassengerUniqueId = stream.getVarLong();
    mHasData = EntityCodec::readPassengerOfBlock(stream, mData);
}

void SetPassengerOfBlockPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
