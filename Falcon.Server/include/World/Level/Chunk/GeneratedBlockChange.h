#pragma once

#include "World/Level/Block/BlockState.h"

#include <cstdint>

struct GeneratedBlockChange {
    int32_t mX = 0;
    int32_t mY = 0;
    int32_t mZ = 0;
    // 1 for the second block of the position, the water of a waterlogged block
    int32_t mLayer = 0;
    // interned (SubChunk::intern), so a change is small to keep and to copy
    const BlockState *mState = nullptr;
};
