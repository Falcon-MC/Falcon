#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class AttributeOperation {
    Addition,
    MultiplyBase,
    MultiplyTotal,
    Cap,
    Invalid,
};

class AttributeModifierData {
public:
    std::string mId;
    std::string mName;
    float mAmount = 0.0f;
    AttributeOperation mOperation = AttributeOperation::Addition;
    int32_t mOperand = 0;
    bool mSerializable = false;
};

class AttributeData {
public:
    std::string mName;
    float mMinimum = 0.0f;
    float mMaximum = 0.0f;
    float mValue = 0.0f;
    float mDefaultMinimum = 0.0f;
    float mDefaultMaximum = 0.0f;
    float mDefaultValue = 0.0f;
    std::vector<AttributeModifierData> mModifiers;
};
