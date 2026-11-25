#pragma once

#include "Protocol/Types/ContainerSlotType.h"
#include "Protocol/Types/FullContainerName.h"

#include <cstdint>

class ItemStackRequestSlotData {
public:
    ContainerSlotType mContainer = ContainerSlotType::Unknown;
    int32_t mSlot = 0;
    int32_t mStackNetworkId = 0;
    FullContainerName mContainerName;
};
