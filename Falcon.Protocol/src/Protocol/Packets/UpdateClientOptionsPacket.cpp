#include "Protocol/Packets/UpdateClientOptionsPacket.h"

#include "Protocol/NetworkPacketHandler.h"

UpdateClientOptionsPacket::UpdateClientOptionsPacket() = default;

void UpdateClientOptionsPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putOptionalPresent(mHasGraphicsMode);
    if (mHasGraphicsMode) {
        stream.putByte((unsigned char) mGraphicsMode);
    }

    stream.putOptionalPresent(mHasFilterProfanityChange);
    if (mHasFilterProfanityChange) {
        stream.putBool(mFilterProfanityChange);
    }
}

void UpdateClientOptionsPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mHasGraphicsMode = stream.getOptionalPresent();
    if (mHasGraphicsMode) {
        mGraphicsMode = (GraphicsMode) stream.getByte();
    }

    mHasFilterProfanityChange = stream.getOptionalPresent();
    if (mHasFilterProfanityChange) {
        mFilterProfanityChange = stream.getBool();
    }
}

void UpdateClientOptionsPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
