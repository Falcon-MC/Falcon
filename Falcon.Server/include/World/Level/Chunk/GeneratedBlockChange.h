#pragma once

#include "World/Level/Block/BlockState.h"

#include <cstdint>

struct GeneratedBlockChange {
    int32_t mX = 0;
    int32_t mY = 0;
    int32_t mZ = 0;
    // 1: second block of the position, the water of a waterlogged block
    int32_t mLayer = 0;
    // interned (SubChunk::intern): a change is cheap to keep and copy
    const BlockState *mState = nullptr;
    // sequence of the decoration placing it, TerrainCache order; 0 unknown: from storage or beyond the 3x3
    uint64_t mSequence = 0;
};

// a chunk decorated as `own` from a copy holding every decoration after `base` already has the block
inline bool decorationHolds(uint64_t base, uint64_t own, uint64_t change) {
    return own != 0 && change > base && change < own;
}
