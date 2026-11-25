#pragma once

#include "Protocol/Types/FullContainerName.h"
#include "Protocol/Types/ItemStackResponseSlot.h"

#include <vector>

class ItemStackResponseContainer {
public:
    FullContainerName mContainerName;
    std::vector<ItemStackResponseSlot> mItems;
};
