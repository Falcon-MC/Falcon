#include "Level/Generator/Feature/FeatureMath.h"

#include "Core/Math/MathConstants.h"

#include <cmath>
#include <vector>

namespace {
    const std::vector<float> &sineTable() {
        static const std::vector<float> table = [] {
            std::vector<float> values(65536);
            for (int32_t i = 0; i < 65536; i++)
                values[i] = (float) std::sin((double) i * MathConstants::PI * 2.0 / 65536.0);

            return values;
        }();

        return table;
    }
}

namespace FeatureMath {
    float sinLookup(float value) {
        return sineTable()[(size_t) (((int32_t) (value * 10430.378f)) & 0xFFFF)];
    }

    float cosLookup(float value) {
        return sineTable()[(size_t) (((int32_t) (value * 10430.378f + 16384.0f)) & 0xFFFF)];
    }

    float sinLookup(double value) {
        return sineTable()[(size_t) (((int32_t) (value * 10430.378f)) & 0xFFFF)];
    }

    float cosLookup(double value) {
        return sineTable()[(size_t) (((int32_t) (value * 10430.378f + 16384.0f)) & 0xFFFF)];
    }

    int32_t floorDouble(double value) {
        const int32_t truncated = (int32_t) value;
        return value < (double) truncated ? truncated - 1 : truncated;
    }

    int32_t clampInt(int32_t value, int32_t min, int32_t max) {
        if (value < min)
            return min;

        if (value > max)
            return max;

        return value;
    }
}
