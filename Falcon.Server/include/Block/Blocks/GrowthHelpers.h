#pragma once

#include "Block/Block.h"

#include <string>

namespace GrowthHelpers {
    const int MINIMUM_LIGHT_LEVEL = 9;

    BlockState withState(const BlockState &state, const std::string &name, int32_t value);
}
