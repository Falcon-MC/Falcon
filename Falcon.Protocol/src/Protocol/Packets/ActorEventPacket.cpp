#include "Protocol/Packets/ActorEventPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ActorEventPacket::ActorEventPacket() = default;

void ActorEventPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarLong(mRuntimeActorId);
    stream.putByte(mEventId);
    stream.putVarInt(mEventData);

    stream.putOptionalPresent(mHasFirePosition);
    if (mHasFirePosition)
        stream.putVector3f(mFirePosition);
}

void ActorEventPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mRuntimeActorId = stream.getUnsignedVarLong();
    mEventId = stream.getByte();
    mEventData = stream.getVarInt();

    mHasFirePosition = stream.getBool();
    if (mHasFirePosition)
        mFirePosition = stream.getVector3f();
}

void ActorEventPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
