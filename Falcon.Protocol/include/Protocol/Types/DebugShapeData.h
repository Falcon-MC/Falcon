#pragma once

#include "Core/Math/Vector3f.h"

#include <cstdint>
#include <string>

enum class DebugShapeType {
    Line,
    Box,
    Sphere,
    Circle,
    Text,
    Arrow,
    Cylinder,
    Pyramid,
    Ellipsoid,
    Cone,
};

class DebugShapeData {
public:
    uint64_t mId = 0;

    bool mHasType = false;
    DebugShapeType mType = DebugShapeType::Line;

    bool mHasPosition = false;
    Vector3f mPosition;

    bool mHasScale = false;
    float mScale = 0.0f;

    bool mHasRotation = false;
    Vector3f mRotation;

    bool mHasTotalTimeLeft = false;
    float mTotalTimeLeft = 0.0f;

    bool mHasMaximumRenderDistance = false;
    float mMaximumRenderDistance = 0.0f;

    bool mHasColor = false;
    uint32_t mColor = 0;

    bool mHasDimension = false;
    int32_t mDimension = -1;

    bool mHasAttachedToActorId = false;
    uint64_t mAttachedToActorId = 0;

    bool mHasArrowEndPosition = false;
    Vector3f mArrowEndPosition;

    bool mHasArrowHeadLength = false;
    float mArrowHeadLength = 0.0f;

    bool mHasArrowHeadRadius = false;
    float mArrowHeadRadius = 0.0f;

    bool mHasArrowHeadSegments = false;
    int32_t mArrowHeadSegments = 0;

    Vector3f mBoxBounds;

    int32_t mSegments = 0;

    Vector3f mLineEndPosition;

    std::string mText;
    bool mUseRotation = false;
    bool mHasBackgroundColor = false;
    uint32_t mBackgroundColor = 0;
    float mLineGapHeight = 0.0f;
    bool mDepthTest = false;
    bool mShowBackface = false;
    bool mShowTextBackface = false;

    Vector2f mRadiusX;
    Vector2f mRadiusZ;
    float mHeight = 0.0f;

    float mWidth = 0.0f;
    bool mHasDepth = false;
    float mDepth = 0.0f;

    Vector2f mRadii2f;
    Vector3f mRadii3f;
};
