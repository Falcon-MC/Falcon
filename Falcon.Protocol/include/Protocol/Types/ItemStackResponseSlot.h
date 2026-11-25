#pragma once

#include <cstdint>
#include <string>

class ItemStackResponseSlot {
public:
    int32_t mSlot = 0;
    int32_t mHotbarSlot = 0;
    int32_t mCount = 0;
    int32_t mStackNetworkId = 0;
    std::string mCustomName;
    int32_t mDurabilityCorrection = 0;
    std::string mFilteredCustomName;
};
