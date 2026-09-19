#pragma once

#include "Block/BlockState.h"

#include <cstdint>

namespace BlockLightProperties {

    int32_t packed(const BlockState &state);

    inline int lightLevel(int32_t packed) {
        return packed & 0xFF;
    }

    inline int lightFilter(int32_t packed) {
        return (packed >> 8) & 0xFF;
    }

    inline bool diffusesSkyLight(int32_t packed) {
        return (packed & (1 << 16)) != 0;
    }

    inline bool isTransparent(int32_t packed) {
        return (packed & (1 << 17)) != 0;
    }

}
