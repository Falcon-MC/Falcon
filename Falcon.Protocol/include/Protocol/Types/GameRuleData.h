#pragma once

#include <cstdint>
#include <string>

enum class ChangedGameRuleType : int32_t {
    Bool = 1,
    Int = 2,
    Float = 3,
};

class ChangedGameRuleData {
public:
    std::string mName;
    bool mEditable = false;
    ChangedGameRuleType mType = ChangedGameRuleType::Bool;

    bool mBoolValue = false;
    int32_t mIntValue = 0;
    float mFloatValue = 0.0f;
};
