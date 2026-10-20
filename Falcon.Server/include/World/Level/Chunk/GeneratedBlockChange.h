#pragma once

#include "World/Level/Block/BlockState.h"

#include <cstdint>

struct GeneratedBlockChange {
    int32_t mX = 0;
    int32_t mY = 0;
    int32_t mZ = 0;
    BlockState mState;
};
