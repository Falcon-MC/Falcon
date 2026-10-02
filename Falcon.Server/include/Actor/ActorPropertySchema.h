#pragma once

#include "Core/Json/Json.h"

#include <cstdint>
#include <string>
#include <vector>

struct ActorPropertyDescription {
    enum class Type {
        Int,
        Float,
        Bool,
        Enum
    };

    std::string mName;
    Type mType = Type::Int;
    int32_t mIndex = 0;
    int32_t mMinInt = 0;
    int32_t mMaxInt = 0;
    int32_t mDefaultInt = 0;
    float mMinFloat = 0.0f;
    float mMaxFloat = 0.0f;
    float mDefaultFloat = 0.0f;
    bool mDefaultBool = false;
    bool mClientSync = false;
    std::string mDefaultExpression;
    std::vector<std::string> mEnumValues;

    int32_t findEnumIndex(const std::string &value) const;
};

class ActorPropertySchema {
public:
    static std::vector<ActorPropertyDescription> parse(const json::Value &properties);

    static const ActorPropertyDescription *find(const std::vector<ActorPropertyDescription> &schema,
                                                const std::string &name);
};
