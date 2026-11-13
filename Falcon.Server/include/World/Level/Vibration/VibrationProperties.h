#pragma once

#include <cstdint>

class BlockState;

// the block's vibration flags as the game packs them: dampen | occlude << 1
namespace VibrationProperties {
    constexpr uint8_t DAMPENS = 0x1;
    constexpr uint8_t OCCLUDES = 0x2;

    // vanilla class first, then a pack block's minecraft:vibration_properties
    uint8_t of(const BlockState &state);
}
