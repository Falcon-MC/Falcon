#pragma once

#include "Protocol/Types/CameraTypes.h"

#include <cstdint>
#include <string>
#include <vector>

enum class AttributeLayerWeightType {
    Float,
    String,
};

class AttributeLayerWeight {
public:
    AttributeLayerWeightType mType = AttributeLayerWeightType::Float;
    float mFloatValue = 0.0f;
    std::string mStringValue;
};

class AttributeLayerSettings {
public:
    int32_t mPriority = 0;
    AttributeLayerWeight mWeight;
    bool mEnabled = false;
    bool mTransitionsPaused = false;
};

enum class Color255Type {
    String,
    Array,
};

class Color255RGBA {
public:
    Color255Type mType = Color255Type::String;
    std::string mStringValue;
    int32_t mArrayValue[4] = {0, 0, 0, 0};
};

enum class EnvironmentAttributeValueType {
    Bool,
    Float,
    Color,
};

class EnvironmentAttributeValue {
public:
    EnvironmentAttributeValueType mType = EnvironmentAttributeValueType::Bool;

    bool mBoolValue = false;
    int32_t mBoolOperation = 0;

    float mFloatValue = 0.0f;
    int32_t mFloatOperation = 0;
    bool mHasConstraintMin = false;
    float mConstraintMin = 0.0f;
    bool mHasConstraintMax = false;
    float mConstraintMax = 0.0f;

    Color255RGBA mColorValue;
    int32_t mColorOperation = 0;
};

enum class NoiseAlignmentType {
    MinLocalTransitionEnd,
};

class NoiseAlignment {
public:
    NoiseAlignmentType mType = NoiseAlignmentType::MinLocalTransitionEnd;
    int32_t mValue = 0;
};

class EnvironmentAttributeData {
public:
    std::string mAttributeName;

    bool mHasFrom = false;
    EnvironmentAttributeValue mFrom;

    EnvironmentAttributeValue mAttribute;

    bool mHasTo = false;
    EnvironmentAttributeValue mTo;

    int32_t mCurrentTransitionTicks = 0;
    int32_t mTotalTransitionTicks = 0;
    CameraEase mEasing = CameraEase::Linear;
    int32_t mLocalTransitionTicks = 0;
    bool mNoiseTransition = false;
    NoiseAlignment mNoiseAlignment;
};

class AttributeLayerData {
public:
    std::string mLayerName;
    int32_t mDimension = 0;
    AttributeLayerSettings mSettings;
    std::vector<EnvironmentAttributeData> mAttributes;
};

enum class AttributeLayerSyncPayloadType {
    UpdateAttributeLayers,
    UpdateAttributeLayerSettings,
    UpdateEnvironmentAttributes,
    RemoveEnvironmentAttributes,
};

class AttributeLayerSyncPayload {
public:
    AttributeLayerSyncPayloadType mType = AttributeLayerSyncPayloadType::UpdateAttributeLayers;

    std::vector<AttributeLayerData> mLayers;

    std::string mLayerName;
    int32_t mDimension = 0;
    AttributeLayerSettings mSettings;
    std::vector<EnvironmentAttributeData> mAttributes;
    std::vector<std::string> mRemovedAttributes;
};
