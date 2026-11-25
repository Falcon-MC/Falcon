#include "Protocol/Packets/MotionPredictionHintsPacket.h"

#include "Protocol/NetworkPacketHandler.h"

MotionPredictionHintsPacket::MotionPredictionHintsPacket() = default;

void MotionPredictionHintsPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarLong(mRuntimeActorId);
    stream.putVector3f(mMotion);
    stream.putBool(mOnGround);
}

void MotionPredictionHintsPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mRuntimeActorId = stream.getUnsignedVarLong();
    mMotion = stream.getVector3f();
    mOnGround = stream.getBool();
}

void MotionPredictionHintsPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
