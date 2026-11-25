#include "Protocol/Packets/DisconnectPacket.h"

#include "Protocol/NetworkPacketHandler.h"

DisconnectPacket::DisconnectPacket()
        : mReason(0), mMessageSkipped(false) {}

void DisconnectPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarInt(mReason);
    stream.putUnsignedVarInt(mMessageSkipped ? 1 : 0);

    if (!mMessageSkipped) {
        stream.putString(mKickMessage);
        stream.putString(mFilteredMessage);
    }
}

void DisconnectPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mReason = stream.getVarInt();
    mMessageSkipped = stream.getUnsignedVarInt() != 0;

    if (!mMessageSkipped) {
        mKickMessage = stream.getString();
        mFilteredMessage = stream.getString();
    }
}

void DisconnectPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
