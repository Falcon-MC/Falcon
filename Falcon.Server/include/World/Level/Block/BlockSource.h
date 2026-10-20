#pragma once

#include "World/Level/Block/BlockState.h"

#include <cstdint>

class BlockSource {
public:
    virtual ~BlockSource() = default;

    virtual BlockState getBlock(int32_t x, int32_t y, int32_t z) const = 0;

    virtual void setBlock(int32_t x, int32_t y, int32_t z, const BlockState &state) = 0;
};
