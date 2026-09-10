#include "Protocol/CameraCodec.h"

void CameraCodec::writeEase(BinaryStream &stream, const CameraEaseData &ease) {
    stream.putOptionalPresent(ease.mHasValue);
    if (ease.mHasValue) {
        stream.putByte((unsigned char) ease.mEaseType);
        stream.putLFloat(ease.mTime);
    }
}

CameraEaseData CameraCodec::readEase(ReadOnlyBinaryStream &stream) {
    CameraEaseData ease;
    ease.mHasValue = stream.getOptionalPresent();
    if (ease.mHasValue) {
        ease.mEaseType = (CameraEase) stream.getByte();
        ease.mTime = stream.getLFloat();
    }
    return ease;
}

void CameraCodec::writeSetInstruction(BinaryStream &stream, const CameraSetInstruction &instruction) {
    stream.putLInt((uint32_t) instruction.mPresetRuntimeId);

    writeEase(stream, instruction.mEase);

    stream.putOptionalPresent(instruction.mHasPos);
    if (instruction.mHasPos) {
        stream.putVector3f(instruction.mPos);
    }

    stream.putOptionalPresent(instruction.mHasRot);
    if (instruction.mHasRot) {
        stream.putVector2f(instruction.mRot);
    }

    stream.putOptionalPresent(instruction.mHasFacing);
    if (instruction.mHasFacing) {
        stream.putVector3f(instruction.mFacing);
    }

    stream.putOptionalPresent(instruction.mHasViewOffset);
    if (instruction.mHasViewOffset) {
        stream.putVector2f(instruction.mViewOffset);
    }

    stream.putOptionalPresent(instruction.mHasActorOffset);
    if (instruction.mHasActorOffset) {
        stream.putVector3f(instruction.mActorOffset);
    }

    stream.putBool(instruction.mHasDefaultPreset);
    if (instruction.mHasDefaultPreset) {
        stream.putBool(instruction.mDefaultPreset);
    }

    stream.putBool(instruction.mRemoveIgnoreStartingValues);
}

CameraSetInstruction CameraCodec::readSetInstruction(ReadOnlyBinaryStream &stream) {
    CameraSetInstruction instruction;
    instruction.mPresetRuntimeId = (int32_t) stream.getLInt();

    instruction.mEase = readEase(stream);

    instruction.mHasPos = stream.getOptionalPresent();
    if (instruction.mHasPos) {
        instruction.mPos = stream.getVector3f();
    }

    instruction.mHasRot = stream.getOptionalPresent();
    if (instruction.mHasRot) {
        instruction.mRot = stream.getVector2f();
    }

    instruction.mHasFacing = stream.getOptionalPresent();
    if (instruction.mHasFacing) {
        instruction.mFacing = stream.getVector3f();
    }

    instruction.mHasViewOffset = stream.getOptionalPresent();
    if (instruction.mHasViewOffset) {
        instruction.mViewOffset = stream.getVector2f();
    }

    instruction.mHasActorOffset = stream.getOptionalPresent();
    if (instruction.mHasActorOffset) {
        instruction.mActorOffset = stream.getVector3f();
    }

    instruction.mHasDefaultPreset = stream.getBool();
    if (instruction.mHasDefaultPreset) {
        instruction.mDefaultPreset = stream.getBool();
    }

    instruction.mRemoveIgnoreStartingValues = stream.getBool();

    return instruction;
}

void CameraCodec::writeFadeInstruction(BinaryStream &stream, const CameraFadeInstruction &instruction) {
    stream.putOptionalPresent(instruction.mHasTimeData);
    if (instruction.mHasTimeData) {
        stream.putLFloat(instruction.mTimeData.mFadeInTime);
        stream.putLFloat(instruction.mTimeData.mWaitTime);
        stream.putLFloat(instruction.mTimeData.mFadeOutTime);
    }

    stream.putOptionalPresent(instruction.mHasColor);
    if (instruction.mHasColor) {
        stream.putLFloat(instruction.mColor.mRed);
        stream.putLFloat(instruction.mColor.mGreen);
        stream.putLFloat(instruction.mColor.mBlue);
    }
}

CameraFadeInstruction CameraCodec::readFadeInstruction(ReadOnlyBinaryStream &stream) {
    CameraFadeInstruction instruction;

    instruction.mHasTimeData = stream.getOptionalPresent();
    if (instruction.mHasTimeData) {
        instruction.mTimeData.mFadeInTime = stream.getLFloat();
        instruction.mTimeData.mWaitTime = stream.getLFloat();
        instruction.mTimeData.mFadeOutTime = stream.getLFloat();
    }

    instruction.mHasColor = stream.getOptionalPresent();
    if (instruction.mHasColor) {
        instruction.mColor.mRed = stream.getLFloat();
        instruction.mColor.mGreen = stream.getLFloat();
        instruction.mColor.mBlue = stream.getLFloat();
    }

    return instruction;
}

void CameraCodec::writeSplineInstruction(BinaryStream &stream, const CameraSplineInstruction &instruction) {
    stream.putLFloat(instruction.mTotalTime);
    stream.putByte((unsigned char) instruction.mType);

    stream.putArrayLength((uint32_t) instruction.mCurve.size());
    for (const Vector3f &point: instruction.mCurve) {
        stream.putVector3f(point);
    }

    stream.putArrayLength((uint32_t) instruction.mProgressKeyFrames.size());
    for (const CameraSplineProgressOption &frame: instruction.mProgressKeyFrames) {
        stream.putLFloat(frame.mValue);
        stream.putLFloat(frame.mTime);
        stream.putByte((unsigned char) frame.mEase);
    }

    stream.putArrayLength((uint32_t) instruction.mRotationOption.size());
    for (const CameraSplineRotationOption &option: instruction.mRotationOption) {
        stream.putVector3f(option.mKeyFrameValues);
        stream.putLFloat(option.mKeyFrameTimes);
        stream.putByte((unsigned char) option.mEase);
    }

    stream.putString(instruction.mSplineIdentifier);
    stream.putBool(instruction.mLoadFromJson);
}

CameraSplineInstruction CameraCodec::readSplineInstruction(ReadOnlyBinaryStream &stream) {
    CameraSplineInstruction instruction;
    instruction.mTotalTime = stream.getLFloat();
    instruction.mType = (CameraSplineType) stream.getByte();

    uint32_t curveLength = stream.getArrayLength();
    instruction.mCurve.reserve(curveLength);
    for (uint32_t i = 0; i < curveLength; i++) {
        instruction.mCurve.push_back(stream.getVector3f());
    }

    uint32_t progressLength = stream.getArrayLength();
    instruction.mProgressKeyFrames.reserve(progressLength);
    for (uint32_t i = 0; i < progressLength; i++) {
        CameraSplineProgressOption frame;
        frame.mValue = stream.getLFloat();
        frame.mTime = stream.getLFloat();
        frame.mEase = (CameraEase) stream.getByte();
        instruction.mProgressKeyFrames.push_back(frame);
    }

    uint32_t rotationLength = stream.getArrayLength();
    instruction.mRotationOption.reserve(rotationLength);
    for (uint32_t i = 0; i < rotationLength; i++) {
        CameraSplineRotationOption option;
        option.mKeyFrameValues = stream.getVector3f();
        option.mKeyFrameTimes = stream.getLFloat();
        option.mEase = (CameraEase) stream.getByte();
        instruction.mRotationOption.push_back(option);
    }

    instruction.mSplineIdentifier = stream.getString();
    instruction.mLoadFromJson = stream.getBool();

    return instruction;
}

void CameraCodec::writeAimAssistPreset(BinaryStream &stream, const CameraAimAssistPreset &preset) {
    stream.putOptionalPresent(preset.mHasIdentifier);
    if (preset.mHasIdentifier) {
        stream.putString(preset.mIdentifier);
    }

    stream.putOptionalPresent(preset.mHasTargetMode);
    if (preset.mHasTargetMode) {
        stream.putLInt((uint32_t) preset.mTargetMode);
    }

    stream.putOptionalPresent(preset.mHasAngle);
    if (preset.mHasAngle) {
        stream.putVector2f(preset.mAngle);
    }

    stream.putOptionalPresent(preset.mHasDistance);
    if (preset.mHasDistance) {
        stream.putLFloat(preset.mDistance);
    }
}

CameraAimAssistPreset CameraCodec::readAimAssistPreset(ReadOnlyBinaryStream &stream) {
    CameraAimAssistPreset preset;

    preset.mHasIdentifier = stream.getOptionalPresent();
    if (preset.mHasIdentifier) {
        preset.mIdentifier = stream.getString();
    }

    preset.mHasTargetMode = stream.getOptionalPresent();
    if (preset.mHasTargetMode) {
        preset.mTargetMode = (int32_t) stream.getLInt();
    }

    preset.mHasAngle = stream.getOptionalPresent();
    if (preset.mHasAngle) {
        preset.mAngle = stream.getVector2f();
    }

    preset.mHasDistance = stream.getOptionalPresent();
    if (preset.mHasDistance) {
        preset.mDistance = stream.getLFloat();
    }

    return preset;
}

void CameraCodec::writePreset(BinaryStream &stream, const CameraPreset &preset) {
    stream.putString(preset.mIdentifier);
    stream.putString(preset.mParentPreset);

    stream.putOptionalPresent(preset.mHasPos);
    if (preset.mHasPos) {
        stream.putLFloat(preset.mPos.x);
    }
    stream.putOptionalPresent(preset.mHasPos);
    if (preset.mHasPos) {
        stream.putLFloat(preset.mPos.y);
    }
    stream.putOptionalPresent(preset.mHasPos);
    if (preset.mHasPos) {
        stream.putLFloat(preset.mPos.z);
    }

    stream.putOptionalPresent(preset.mHasPitch);
    if (preset.mHasPitch) {
        stream.putLFloat(preset.mPitch);
    }

    stream.putOptionalPresent(preset.mHasYaw);
    if (preset.mHasYaw) {
        stream.putLFloat(preset.mYaw);
    }

    stream.putOptionalPresent(preset.mHasRotationSpeed);
    if (preset.mHasRotationSpeed) {
        stream.putLFloat(preset.mRotationSpeed);
    }

    stream.putBool(preset.mHasSnapToTarget);
    if (preset.mHasSnapToTarget) {
        stream.putBool(preset.mSnapToTarget);
    }

    stream.putOptionalPresent(preset.mHasHorizontalRotationLimit);
    if (preset.mHasHorizontalRotationLimit) {
        stream.putVector2f(preset.mHorizontalRotationLimit);
    }

    stream.putOptionalPresent(preset.mHasVerticalRotationLimit);
    if (preset.mHasVerticalRotationLimit) {
        stream.putVector2f(preset.mVerticalRotationLimit);
    }

    stream.putBool(preset.mHasContinueTargeting);
    if (preset.mHasContinueTargeting) {
        stream.putBool(preset.mContinueTargeting);
    }

    stream.putOptionalPresent(preset.mHasBlockListeningRadius);
    if (preset.mHasBlockListeningRadius) {
        stream.putLFloat(preset.mBlockListeningRadius);
    }

    stream.putOptionalPresent(preset.mHasViewOffset);
    if (preset.mHasViewOffset) {
        stream.putVector2f(preset.mViewOffset);
    }

    stream.putOptionalPresent(preset.mHasActorOffset);
    if (preset.mHasActorOffset) {
        stream.putVector3f(preset.mActorOffset);
    }

    stream.putOptionalPresent(preset.mHasRadius);
    if (preset.mHasRadius) {
        stream.putLFloat(preset.mRadius);
    }

    stream.putOptionalPresent(preset.mHasMinYawLimit);
    if (preset.mHasMinYawLimit) {
        stream.putLFloat(preset.mMinYawLimit);
    }

    stream.putOptionalPresent(preset.mHasMaxYawLimit);
    if (preset.mHasMaxYawLimit) {
        stream.putLFloat(preset.mMaxYawLimit);
    }

    stream.putOptionalPresent(preset.mHasListener);
    if (preset.mHasListener) {
        stream.putByte((unsigned char) preset.mListener);
    }

    stream.putBool(preset.mHasPlayEffect);
    if (preset.mHasPlayEffect) {
        stream.putBool(preset.mPlayEffect);
    }

    stream.putOptionalPresent(preset.mHasAimAssistPreset);
    if (preset.mHasAimAssistPreset) {
        writeAimAssistPreset(stream, preset.mAimAssistPreset);
    }

    stream.putOptionalPresent(preset.mHasControlScheme);
    if (preset.mHasControlScheme) {
        stream.putByte((unsigned char) preset.mControlScheme);
    }

    stream.putBool(preset.mApplyInheritedStartingRotation);

    stream.putOptionalPresent(preset.mHasStartingRotation);
    if (preset.mHasStartingRotation) {
        stream.putVector2f(preset.mStartingRotation);
    }
}

CameraPreset CameraCodec::readPreset(ReadOnlyBinaryStream &stream) {
    CameraPreset preset;
    preset.mIdentifier = stream.getString();
    preset.mParentPreset = stream.getString();

    bool hasX = stream.getOptionalPresent();
    float x = hasX ? stream.getLFloat() : 0.0f;
    bool hasY = stream.getOptionalPresent();
    float y = hasY ? stream.getLFloat() : 0.0f;
    bool hasZ = stream.getOptionalPresent();
    float z = hasZ ? stream.getLFloat() : 0.0f;
    preset.mHasPos = hasX && hasY && hasZ;
    if (preset.mHasPos) {
        preset.mPos = Vector3f(x, y, z);
    }

    preset.mHasPitch = stream.getOptionalPresent();
    if (preset.mHasPitch) {
        preset.mPitch = stream.getLFloat();
    }

    preset.mHasYaw = stream.getOptionalPresent();
    if (preset.mHasYaw) {
        preset.mYaw = stream.getLFloat();
    }

    preset.mHasRotationSpeed = stream.getOptionalPresent();
    if (preset.mHasRotationSpeed) {
        preset.mRotationSpeed = stream.getLFloat();
    }

    preset.mHasSnapToTarget = stream.getBool();
    if (preset.mHasSnapToTarget) {
        preset.mSnapToTarget = stream.getBool();
    }

    preset.mHasHorizontalRotationLimit = stream.getOptionalPresent();
    if (preset.mHasHorizontalRotationLimit) {
        preset.mHorizontalRotationLimit = stream.getVector2f();
    }

    preset.mHasVerticalRotationLimit = stream.getOptionalPresent();
    if (preset.mHasVerticalRotationLimit) {
        preset.mVerticalRotationLimit = stream.getVector2f();
    }

    preset.mHasContinueTargeting = stream.getBool();
    if (preset.mHasContinueTargeting) {
        preset.mContinueTargeting = stream.getBool();
    }

    preset.mHasBlockListeningRadius = stream.getOptionalPresent();
    if (preset.mHasBlockListeningRadius) {
        preset.mBlockListeningRadius = stream.getLFloat();
    }

    preset.mHasViewOffset = stream.getOptionalPresent();
    if (preset.mHasViewOffset) {
        preset.mViewOffset = stream.getVector2f();
    }

    preset.mHasActorOffset = stream.getOptionalPresent();
    if (preset.mHasActorOffset) {
        preset.mActorOffset = stream.getVector3f();
    }

    preset.mHasRadius = stream.getOptionalPresent();
    if (preset.mHasRadius) {
        preset.mRadius = stream.getLFloat();
    }

    preset.mHasMinYawLimit = stream.getOptionalPresent();
    if (preset.mHasMinYawLimit) {
        preset.mMinYawLimit = stream.getLFloat();
    }

    preset.mHasMaxYawLimit = stream.getOptionalPresent();
    if (preset.mHasMaxYawLimit) {
        preset.mMaxYawLimit = stream.getLFloat();
    }

    preset.mHasListener = stream.getOptionalPresent();
    if (preset.mHasListener) {
        preset.mListener = (CameraAudioListener) stream.getByte();
    }

    preset.mHasPlayEffect = stream.getBool();
    if (preset.mHasPlayEffect) {
        preset.mPlayEffect = stream.getBool();
    }

    preset.mHasAimAssistPreset = stream.getOptionalPresent();
    if (preset.mHasAimAssistPreset) {
        preset.mAimAssistPreset = readAimAssistPreset(stream);
    }

    preset.mHasControlScheme = stream.getOptionalPresent();
    if (preset.mHasControlScheme) {
        preset.mControlScheme = (ControlScheme) stream.getByte();
    }

    preset.mApplyInheritedStartingRotation = stream.getBool();

    preset.mHasStartingRotation = stream.getOptionalPresent();
    if (preset.mHasStartingRotation) {
        preset.mStartingRotation = stream.getVector2f();
    }

    return preset;
}
