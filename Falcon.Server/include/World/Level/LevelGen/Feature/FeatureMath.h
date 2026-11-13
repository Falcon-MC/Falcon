#pragma once

#include <cstdint>

namespace FeatureMath {
    float sinLookup(float value);

    float cosLookup(float value);

    float sinLookup(double value);

    float cosLookup(double value);

    // the game's sine table: sinf(i / 10430.378f), indexed by an angle pre-scaled by 10430.378f;
    // two thirds of its entries differ by an ulp from sinLookup's double-filled table: game-exact ports read this one
    float sineAt(float scaled);

    int32_t floorDouble(double value);

    int32_t clampInt(int32_t value, int32_t min, int32_t max);
}
