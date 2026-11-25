#pragma once

#include <cstdint>

enum class EntityLinkType {
    Remove,
    Rider,
    Passenger,
};

class EntityLinkData {
public:
    int64_t mFrom = 0;
    int64_t mTo = 0;
    EntityLinkType mType = EntityLinkType::Remove;
    bool mImmediate = false;
    bool mRiderInitiated = false;
    float mVehicleAngularVelocity = 0.0f;
};
