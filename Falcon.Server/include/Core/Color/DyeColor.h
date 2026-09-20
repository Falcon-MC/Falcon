#pragma once

#include <cstdint>
#include <string>

namespace DyeColor {
    constexpr uint8_t COUNT = 16;

    constexpr uint8_t INVALID = 0xFF;

    const char *name(uint8_t color);

    uint8_t fromName(const std::string &name);

    const char *identifier(uint8_t color, const char *suffix);

    uint8_t fromIdentifier(const std::string &identifier, const char *suffix);

    uint8_t invert(uint8_t color);

    uint8_t clamp(uint8_t color);
}
