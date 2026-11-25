#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>

enum class PassengerOfBlockEmoteType : uint8_t {
    Standing = 0,
    Riding = 1,
    Laying = 2
};

class PassengerOfBlockData {
public:
    Vector3i mBlockPosition;
    Vector3f mOffset;
    float mRotation = 0.0f;
    float mRotationLimit = 0.0f;
    PassengerOfBlockEmoteType mEmoteType = PassengerOfBlockEmoteType::Standing;
};
