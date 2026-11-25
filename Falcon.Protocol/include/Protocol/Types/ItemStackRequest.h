#pragma once

#include "Protocol/Types/ItemStackRequestAction.h"

#include <cstdint>
#include <string>
#include <vector>

class ItemStackRequest {
public:
    int32_t mRequestId = 0;
    std::vector<ItemStackRequestAction> mActions;
    std::vector<std::string> mFilterStrings;
    bool mHasTextProcessingEventOrigin = false;
    int32_t mTextProcessingEventOrigin = 0;
};
