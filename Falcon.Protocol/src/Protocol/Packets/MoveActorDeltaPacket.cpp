#include "Protocol/Packets/MoveActorDeltaPacket.h"

#include "Protocol/NetworkPacketHandler.h"

MoveActorDeltaPacket::MoveActorDeltaPacket() = default;

void MoveActorDeltaPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarLong(mRuntimeActorId);

    stream.putOptionalPresent(mHasX);
    if (mHasX)
        stream.putLFloat(mX);

    stream.putOptionalPresent(mHasY);
    if (mHasY)
        stream.putLFloat(mY);

    stream.putOptionalPresent(mHasZ);
    if (mHasZ)
        stream.putLFloat(mZ);

    stream.putOptionalPresent(mHasPitch);
    if (mHasPitch)
        stream.putByte((unsigned char) (mPitch * 256.0f / 360.0f));

    stream.putOptionalPresent(mHasYaw);
    if (mHasYaw)
        stream.putByte((unsigned char) (mYaw * 256.0f / 360.0f));

    stream.putOptionalPresent(mHasHeadYaw);
    if (mHasHeadYaw)
        stream.putByte((unsigned char) (mHeadYaw * 256.0f / 360.0f));

    stream.putBool(mOnGround);
    stream.putBool(mForceMove);
    stream.putBool(mForceMoveLocalActor);
    stream.putBool(mForceCompletion);
    stream.putUnsignedVarLong(mTicks);
}

void MoveActorDeltaPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mRuntimeActorId = stream.getUnsignedVarLong();

    mHasX = stream.getOptionalPresent();
    if (mHasX)
        mX = stream.getLFloat();

    mHasY = stream.getOptionalPresent();
    if (mHasY)
        mY = stream.getLFloat();

    mHasZ = stream.getOptionalPresent();
    if (mHasZ)
        mZ = stream.getLFloat();

    mHasPitch = stream.getOptionalPresent();
    if (mHasPitch)
        mPitch = stream.getByte() * 360.0f / 256.0f;

    mHasYaw = stream.getOptionalPresent();
    if (mHasYaw)
        mYaw = stream.getByte() * 360.0f / 256.0f;

    mHasHeadYaw = stream.getOptionalPresent();
    if (mHasHeadYaw)
        mHeadYaw = stream.getByte() * 360.0f / 256.0f;

    mOnGround = stream.getBool();
    mForceMove = stream.getBool();
    mForceMoveLocalActor = stream.getBool();
    mForceCompletion = stream.getBool();
    mTicks = stream.getUnsignedVarLong();
}

void MoveActorDeltaPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
