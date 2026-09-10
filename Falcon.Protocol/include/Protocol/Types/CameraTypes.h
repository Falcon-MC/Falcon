#pragma once

#include "Core/Math/Vector3f.h"

#include <cstdint>
#include <string>
#include <vector>

enum class CameraEase {
    Linear,
    Spring,
    EaseInSine,
    EaseOutSine,
    EaseInOutSine,
    EaseInQuad,
    EaseOutQuad,
    EaseInOutQuad,
    EaseInCubic,
    EaseOutCubic,
    EaseInOutCubic,
    EaseInQuart,
    EaseOutQuart,
    EaseInOutQuart,
    EaseInQuint,
    EaseOutQuint,
    EaseInOutQuint,
    EaseInExpo,
    EaseOutExpo,
    EaseInOutExpo,
    EaseInCirc,
    EaseOutCirc,
    EaseInOutCirc,
    EaseInBack,
    EaseOutBack,
    EaseInOutBack,
    EaseInElastic,
    EaseOutElastic,
    EaseInOutElastic,
    EaseInBounce,
    EaseOutBounce,
    EaseInOutBounce,
};

enum class CameraSplineType {
    CatmullRom,
    Linear,
};

enum class CameraAudioListener {
    Camera,
    Player,
};

enum class ControlScheme {
    LockedPlayerRelativeStrafe,
    CameraRelative,
    CameraRelativeStrafe,
    PlayerRelative,
    PlayerRelativeStrafe,
};

class CameraEaseData {
public:
    bool mHasValue = false;
    CameraEase mEaseType = CameraEase::Linear;
    float mTime = 0.0f;
};

class CameraSetInstruction {
public:
    int32_t mPresetRuntimeId = 0;

    CameraEaseData mEase;

    bool mHasPos = false;
    Vector3f mPos;

    bool mHasRot = false;
    Vector2f mRot;

    bool mHasFacing = false;
    Vector3f mFacing;

    bool mHasViewOffset = false;
    Vector2f mViewOffset;

    bool mHasActorOffset = false;
    Vector3f mActorOffset;

    bool mHasDefaultPreset = false;
    bool mDefaultPreset = false;

    bool mRemoveIgnoreStartingValues = false;
};

class CameraFadeTimeData {
public:
    float mFadeInTime = 0.0f;
    float mWaitTime = 0.0f;
    float mFadeOutTime = 0.0f;
};

class CameraFadeColor {
public:
    float mRed = 0.0f;
    float mGreen = 0.0f;
    float mBlue = 0.0f;
};

class CameraFadeInstruction {
public:
    bool mHasTimeData = false;
    CameraFadeTimeData mTimeData;

    bool mHasColor = false;
    CameraFadeColor mColor;
};

class CameraTargetInstruction {
public:
    bool mHasTargetCenterOffset = false;
    Vector3f mTargetCenterOffset;
    int64_t mUniqueActorId = 0;
};

class CameraFovInstruction {
public:
    float mFov = 0.0f;
    float mEaseTime = 0.0f;
    CameraEase mEaseType = CameraEase::Linear;
    bool mClear = false;
};

class CameraAttachToEntityInstruction {
public:
    int64_t mUniqueActorId = 0;
};

class CameraSplineProgressOption {
public:
    float mValue = 0.0f;
    float mTime = 0.0f;
    CameraEase mEase = CameraEase::Linear;
};

class CameraSplineRotationOption {
public:
    Vector3f mKeyFrameValues;
    float mKeyFrameTimes = 0.0f;
    CameraEase mEase = CameraEase::Linear;
};

class CameraSplineInstruction {
public:
    float mTotalTime = 0.0f;
    CameraSplineType mType = CameraSplineType::Linear;
    std::vector<Vector3f> mCurve;
    std::vector<CameraSplineProgressOption> mProgressKeyFrames;
    std::vector<CameraSplineRotationOption> mRotationOption;
    std::string mSplineIdentifier;
    bool mLoadFromJson = false;
};

class CameraSplineDefinition {
public:
    std::string mName;
    CameraSplineInstruction mInstruction;
};

class CameraAimAssistPreset {
public:
    bool mHasIdentifier = false;
    std::string mIdentifier;

    bool mHasTargetMode = false;
    int32_t mTargetMode = 0;

    bool mHasAngle = false;
    Vector2f mAngle;

    bool mHasDistance = false;
    float mDistance = 0.0f;
};

class CameraPreset {
public:
    std::string mIdentifier;
    std::string mParentPreset;

    bool mHasPos = false;
    Vector3f mPos;

    bool mHasPitch = false;
    float mPitch = 0.0f;

    bool mHasYaw = false;
    float mYaw = 0.0f;

    bool mHasRotationSpeed = false;
    float mRotationSpeed = 0.0f;

    bool mHasSnapToTarget = false;
    bool mSnapToTarget = false;

    bool mHasHorizontalRotationLimit = false;
    Vector2f mHorizontalRotationLimit;

    bool mHasVerticalRotationLimit = false;
    Vector2f mVerticalRotationLimit;

    bool mHasContinueTargeting = false;
    bool mContinueTargeting = false;

    bool mHasBlockListeningRadius = false;
    float mBlockListeningRadius = 0.0f;

    bool mHasViewOffset = false;
    Vector2f mViewOffset;

    bool mHasActorOffset = false;
    Vector3f mActorOffset;

    bool mHasRadius = false;
    float mRadius = 0.0f;

    bool mHasMinYawLimit = false;
    float mMinYawLimit = 0.0f;

    bool mHasMaxYawLimit = false;
    float mMaxYawLimit = 0.0f;

    bool mHasListener = false;
    CameraAudioListener mListener = CameraAudioListener::Camera;

    bool mHasPlayEffect = false;
    bool mPlayEffect = false;

    bool mHasAimAssistPreset = false;
    CameraAimAssistPreset mAimAssistPreset;

    bool mHasControlScheme = false;
    ControlScheme mControlScheme = ControlScheme::LockedPlayerRelativeStrafe;

    bool mApplyInheritedStartingRotation = false;
    bool mHasStartingRotation = false;
    Vector2f mStartingRotation;
};
