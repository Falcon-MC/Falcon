#pragma once

#include <cstdint>
#include <vector>

class AbilityLayer {
public:
    uint16_t mLayerType = 0;
    uint32_t mAbilitiesSet = 0;
    uint32_t mAbilityValues = 0;
    float mFlySpeed = 0.0f;
    float mVerticalFlySpeed = 0.0f;
    float mWalkSpeed = 0.0f;
};

class PlayerAbilityData {
public:
    int64_t mUniqueActorId = 0;
    uint8_t mPlayerPermission = 0;
    uint8_t mCommandPermission = 0;
    std::vector<AbilityLayer> mAbilityLayers;
};
