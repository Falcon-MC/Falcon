#include "Loot/LootRange.h"

#include <cmath>

LootRange LootRange::parse(const json::Value *value, float fallback) {
    LootRange range{fallback, fallback};
    if (value == nullptr)
        return range;

    if (value->isNumber()) {
        range.mMin = (float) value->mNumber;
        range.mMax = range.mMin;
        return range;
    }

    if (value->isObject()) {
        const json::Value *minimum = value->get("min");
        const json::Value *maximum = value->get("max");
        range.mMin = minimum == nullptr ? fallback : (float) minimum->number(fallback);
        range.mMax = maximum == nullptr ? range.mMin : (float) maximum->number(range.mMin);
    }

    return range;
}

int32_t LootRange::rollInt(std::mt19937 &random) const {
    const int32_t minimum = (int32_t) std::floor(mMin);
    const int32_t maximum = (int32_t) std::floor(mMax);
    if (maximum <= minimum)
        return minimum;

    return std::uniform_int_distribution<int32_t>(minimum, maximum)(random);
}

float LootRange::rollFloat(std::mt19937 &random) const {
    if (mMax <= mMin)
        return mMin;

    return std::uniform_real_distribution<float>(mMin, mMax)(random);
}
