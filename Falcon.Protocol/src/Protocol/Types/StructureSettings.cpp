#include "Protocol/Types/StructureSettings.h"

void StructureSettings::write(BinaryStream &stream) const {
    stream.putString(mPaletteName);
    stream.putBool(mIgnoringEntities);
    stream.putBool(mIgnoringBlocks);
    stream.putBool(mNonTickingPlayersAndTickingAreasEnabled);
    stream.putBlockPosition(mSize);
    stream.putBlockPosition(mOffset);
    stream.putVarLong(mLastEditedByActorId);
    stream.putByte((unsigned char) mRotation);
    stream.putByte((unsigned char) mMirror);
    stream.putByte((unsigned char) mAnimationMode);
    stream.putLFloat(mAnimationSeconds);
    stream.putLFloat(mIntegrityValue);
    stream.putLInt((uint32_t) mIntegritySeed);
    stream.putVector3f(mPivot);
}

void StructureSettings::read(ReadOnlyBinaryStream &stream) {
    mPaletteName = stream.getString();
    mIgnoringEntities = stream.getBool();
    mIgnoringBlocks = stream.getBool();
    mNonTickingPlayersAndTickingAreasEnabled = stream.getBool();
    mSize = stream.getBlockPosition();
    mOffset = stream.getBlockPosition();
    mLastEditedByActorId = stream.getVarLong();
    mRotation = (StructureRotation) stream.getByte();
    mMirror = (StructureMirror) stream.getByte();
    mAnimationMode = (StructureAnimationMode) stream.getByte();
    mAnimationSeconds = stream.getLFloat();
    mIntegrityValue = stream.getLFloat();
    mIntegritySeed = stream.getSignedLInt();
    mPivot = stream.getVector3f();
}
