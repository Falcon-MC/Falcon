#pragma once

#include <cstdint>

namespace FeatureMath {
    float sinLookup(float value);

    float cosLookup(float value);

    float sinLookup(double value);

    float cosLookup(double value);

    /**
     * The game's own sine table (sub_14199D950), filled with sinf of i / 10430.378f and indexed by an angle already
     * scaled by 10430.378f. Two thirds of its entries differ by an ulp from sinLookup's table, which is filled in
     * double, so the ports that must match the game read this one.
     */
    float sineAt(float scaled);

    int32_t floorDouble(double value);

    int32_t clampInt(int32_t value, int32_t min, int32_t max);
}
