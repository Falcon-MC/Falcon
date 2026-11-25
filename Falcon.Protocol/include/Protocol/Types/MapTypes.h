#pragma once

#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <string>

enum class MapTrackedObjectType {
    Entity,
    Block,
};

class MapTrackedObject {
public:
    MapTrackedObjectType mType = MapTrackedObjectType::Entity;
    int64_t mActorId = 0;
    Vector3i mPosition;
};

class MapDecoration {
public:
    int32_t mImage = 0;
    int32_t mRotation = 0;
    int32_t mXOffset = 0;
    int32_t mYOffset = 0;
    std::string mLabel;
    int32_t mColor = 0;
};
