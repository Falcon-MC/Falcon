#include "Protocol/Packets/CameraInstructionPacket.h"

#include "Protocol/CameraCodec.h"
#include "Protocol/NetworkPacketHandler.h"

CameraInstructionPacket::CameraInstructionPacket() = default;

void CameraInstructionPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putOptionalPresent(mHasSetInstruction);
    if (mHasSetInstruction) {
        CameraCodec::writeSetInstruction(stream, mSetInstruction);
    }

    stream.putBool(mHasClear);
    if (mHasClear) {
        stream.putBool(mClear);
    }

    stream.putOptionalPresent(mHasFadeInstruction);
    if (mHasFadeInstruction) {
        CameraCodec::writeFadeInstruction(stream, mFadeInstruction);
    }

    stream.putOptionalPresent(mHasTargetInstruction);
    if (mHasTargetInstruction) {
        stream.putOptionalPresent(mTargetInstruction.mHasTargetCenterOffset);
        if (mTargetInstruction.mHasTargetCenterOffset) {
            stream.putVector3f(mTargetInstruction.mTargetCenterOffset);
        }
        stream.putLLong((uint64_t) mTargetInstruction.mUniqueActorId);
    }

    stream.putBool(mHasRemoveTarget);
    if (mHasRemoveTarget) {
        stream.putBool(mRemoveTarget);
    }

    stream.putOptionalPresent(mHasFovInstruction);
    if (mHasFovInstruction) {
        stream.putLFloat(mFovInstruction.mFov);
        stream.putLFloat(mFovInstruction.mEaseTime);
        stream.putByte((unsigned char) mFovInstruction.mEaseType);
        stream.putBool(mFovInstruction.mClear);
    }

    stream.putOptionalPresent(mHasSplineInstruction);
    if (mHasSplineInstruction) {
        CameraCodec::writeSplineInstruction(stream, mSplineInstruction);
    }

    stream.putOptionalPresent(mHasAttachInstruction);
    if (mHasAttachInstruction) {
        stream.putLLong((uint64_t) mAttachInstruction.mUniqueActorId);
    }

    stream.putBool(mHasDetachFromActor);
    if (mHasDetachFromActor) {
        stream.putBool(mDetachFromActor);
    }
}

void CameraInstructionPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mHasSetInstruction = stream.getOptionalPresent();
    if (mHasSetInstruction) {
        mSetInstruction = CameraCodec::readSetInstruction(stream);
    }

    mHasClear = stream.getBool();
    if (mHasClear) {
        mClear = stream.getBool();
    }

    mHasFadeInstruction = stream.getOptionalPresent();
    if (mHasFadeInstruction) {
        mFadeInstruction = CameraCodec::readFadeInstruction(stream);
    }

    mHasTargetInstruction = stream.getOptionalPresent();
    if (mHasTargetInstruction) {
        mTargetInstruction.mHasTargetCenterOffset = stream.getOptionalPresent();
        if (mTargetInstruction.mHasTargetCenterOffset) {
            mTargetInstruction.mTargetCenterOffset = stream.getVector3f();
        }
        mTargetInstruction.mUniqueActorId = (int64_t) stream.getLLong();
    }

    mHasRemoveTarget = stream.getBool();
    if (mHasRemoveTarget) {
        mRemoveTarget = stream.getBool();
    }

    mHasFovInstruction = stream.getOptionalPresent();
    if (mHasFovInstruction) {
        mFovInstruction.mFov = stream.getLFloat();
        mFovInstruction.mEaseTime = stream.getLFloat();
        mFovInstruction.mEaseType = (CameraEase) stream.getByte();
        mFovInstruction.mClear = stream.getBool();
    }

    mHasSplineInstruction = stream.getOptionalPresent();
    if (mHasSplineInstruction) {
        mSplineInstruction = CameraCodec::readSplineInstruction(stream);
    }

    mHasAttachInstruction = stream.getOptionalPresent();
    if (mHasAttachInstruction) {
        mAttachInstruction.mUniqueActorId = (int64_t) stream.getLLong();
    }

    mHasDetachFromActor = stream.getBool();
    if (mHasDetachFromActor) {
        mDetachFromActor = stream.getBool();
    }
}

void CameraInstructionPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
