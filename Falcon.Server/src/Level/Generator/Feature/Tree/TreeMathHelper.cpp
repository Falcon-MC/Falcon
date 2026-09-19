#include "Level/Generator/Feature/Tree/TreeMathHelper.h"

#include "Core/Math/MathConstants.h"

#include <array>
#include <cmath>

namespace {

    const std::array<float, 65536> &sinTable() {
        static const std::array<float, 65536> table = [] {
            std::array<float, 65536> values{};
            for (int32_t i = 0; i < 65536; i++)
                values[(size_t) i] = (float) std::sin((double) i * MathConstants::PI * 2.0 / 65536.0);

            return values;
        }();

        return table;
    }

}

float TreeMathHelper::sin(float value) {
    const int32_t index = (int32_t) (value * 10430.378f) & 0xFFFF;
    return sinTable()[(size_t) index];
}

float TreeMathHelper::cos(float value) {
    const int32_t index = (int32_t) (value * 10430.378f + 16384.0f) & 0xFFFF;
    return sinTable()[(size_t) index];
}

float TreeMathHelper::sin(double value) {
    const int32_t index = (int32_t) (value * (double) 10430.378f) & 0xFFFF;
    return sinTable()[(size_t) index];
}

float TreeMathHelper::cos(double value) {
    const int32_t index = (int32_t) (value * (double) 10430.378f + (double) 16384.0f) & 0xFFFF;
    return sinTable()[(size_t) index];
}
