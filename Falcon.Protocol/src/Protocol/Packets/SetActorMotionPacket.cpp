#include "Protocol/Packets/SetActorMotionPacket.h"

#include "Protocol/NetworkPacketHandler.h"

SetActorMotionPacket::SetActorMotionPacket()
        : mRuntimeActorId(0), mTick(0) {}

void SetActorMotionPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarLong(mRuntimeActorId);
    stream.putVector3f(mMotion);
    stream.putUnsignedVarLong(mTick);
}

void SetActorMotionPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mRuntimeActorId = stream.getUnsignedVarLong();
    mMotion = stream.getVector3f();
    mTick = stream.getUnsignedVarLong();
}

void SetActorMotionPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
