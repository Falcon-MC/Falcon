#include "Protocol/Packets/BlockPickRequestPacket.h"

#include "Protocol/NetworkPacketHandler.h"

BlockPickRequestPacket::BlockPickRequestPacket()
        : mAddUserData(false), mHotbarSlot(0) {}

void BlockPickRequestPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVector3i(mBlockPosition);
    stream.putBool(mAddUserData);
    stream.putByte((unsigned char) mHotbarSlot);
}

void BlockPickRequestPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mBlockPosition = stream.getVector3i();
    mAddUserData = stream.getBool();
    mHotbarSlot = stream.getByte();
}

void BlockPickRequestPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
