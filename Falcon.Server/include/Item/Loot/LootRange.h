#pragma once

#include "Core/Json/Json.h"

#include <cstdint>
#include <random>

struct LootRange {
    float mMin = 0.0f;
    float mMax = 0.0f;

    static LootRange parse(const json::Value *value, float fallback);

    int32_t rollInt(std::mt19937 &random) const;

    float rollFloat(std::mt19937 &random) const;
};
