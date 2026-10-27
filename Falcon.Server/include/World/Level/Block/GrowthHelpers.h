#pragma once

#include "World/Level/Block/Block.h"

#include <string>

namespace GrowthHelpers {
    const int MINIMUM_LIGHT_LEVEL = 9;

    BlockState withState(const BlockState &state, const std::string &name, int32_t value);
}
