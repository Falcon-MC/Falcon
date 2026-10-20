#pragma once

#include <cstdint>
#include <string>

struct DynamicPropertyValue {
    enum class Type {
        Undefined,
        Boolean,
        Number,
        String,
        Vector
    };

    Type mType = Type::Undefined;
    bool mBoolean = false;
    double mNumber = 0.0;
    std::string mString;
    float mVector[3] = {0.0f, 0.0f, 0.0f};
};
