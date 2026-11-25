#include "Protocol/Packets/InteractPacket.h"

#include "Protocol/NetworkPacketHandler.h"

InteractPacket::InteractPacket()
        : mAction(Action::None), mRuntimeActorId(0), mHasMousePosition(false) {}

void InteractPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte((unsigned char) mAction);
    stream.putUnsignedVarLong(mRuntimeActorId);
    stream.putOptionalPresent(mHasMousePosition);
    if (mHasMousePosition)
        stream.putVector3f(mMousePosition);
}

void InteractPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mAction = (Action) stream.getByte();
    mRuntimeActorId = stream.getUnsignedVarLong();
    mHasMousePosition = stream.getOptionalPresent();
    if (mHasMousePosition)
        mMousePosition = stream.getVector3f();
}

void InteractPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
