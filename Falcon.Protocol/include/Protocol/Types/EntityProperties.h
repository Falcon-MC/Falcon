#pragma once

#include <cstdint>
#include <vector>

class IntEntityProperty {
public:
    int32_t mIndex = 0;
    int32_t mValue = 0;
};

class FloatEntityProperty {
public:
    int32_t mIndex = 0;
    float mValue = 0.0f;
};

class EntityProperties {
public:
    std::vector<IntEntityProperty> mIntProperties;
    std::vector<FloatEntityProperty> mFloatProperties;
};
