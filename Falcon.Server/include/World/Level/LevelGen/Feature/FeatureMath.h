#pragma once

#include <cstdint>

namespace FeatureMath {
    float sinLookup(float value);

    float cosLookup(float value);

    float sinLookup(double value);

    float cosLookup(double value);

    int32_t floorDouble(double value);

    int32_t clampInt(int32_t value, int32_t min, int32_t max);
}
