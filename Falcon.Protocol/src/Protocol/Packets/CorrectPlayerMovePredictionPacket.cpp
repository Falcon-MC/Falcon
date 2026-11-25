#include "Protocol/Packets/CorrectPlayerMovePredictionPacket.h"

#include "Protocol/NetworkPacketHandler.h"

CorrectPlayerMovePredictionPacket::CorrectPlayerMovePredictionPacket() = default;

void CorrectPlayerMovePredictionPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte((unsigned char) mPredictionType);
    stream.putVector3f(mPosition);
    stream.putVector3f(mDelta);
    stream.putVector2f(mVehicleRotation);
    stream.putOptionalPresent(mHasVehicleAngularVelocity);
    if (mHasVehicleAngularVelocity)
        stream.putLFloat(mVehicleAngularVelocity);
    stream.putBool(mOnGround);
    stream.putUnsignedVarLong(mTick);
}

void CorrectPlayerMovePredictionPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mPredictionType = (PredictionType) stream.getByte();
    mPosition = stream.getVector3f();
    mDelta = stream.getVector3f();
    mVehicleRotation = stream.getVector2f();
    mHasVehicleAngularVelocity = stream.getOptionalPresent();
    if (mHasVehicleAngularVelocity)
        mVehicleAngularVelocity = stream.getLFloat();
    mOnGround = stream.getBool();
    mTick = stream.getUnsignedVarLong();
}

void CorrectPlayerMovePredictionPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
