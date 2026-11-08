#pragma once

#include "Core/Math/Vector3i.h"
#include "World/Village/PoiBlueprint.h"

#include <cstdint>
#include <string>

// created from its block's blueprint with no owners yet
struct PoiInstance {
    PoiInstance(const Vector3i &position, const PoiBlueprint &blueprint);

    Vector3i mPosition;
    std::string mName;
    std::string mInitEvent;
    std::string mEndEvent;
    PoiType mType;
    uint64_t mOwnerCount;
    uint64_t mCapacity;
    uint64_t mWeight;
};
