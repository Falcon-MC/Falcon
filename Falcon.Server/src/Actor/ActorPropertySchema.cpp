#include "Actor/ActorPropertySchema.h"

#include <algorithm>

namespace {
    bool isExpression(const json::Value *value) {
        return value != nullptr && value->isString();
    }
}

int32_t ActorPropertyDescription::findEnumIndex(const std::string &value) const {
    for (size_t index = 0; index < mEnumValues.size(); ++index) {
        if (mEnumValues[index] == value)
            return (int32_t) index;
    }
    return -1;
}

std::vector<ActorPropertyDescription> ActorPropertySchema::parse(const json::Value &properties) {
    std::vector<ActorPropertyDescription> schema;
    if (!properties.isObject())
        return schema;

    std::vector<std::string> names = properties.mKeys;
    std::sort(names.begin(), names.end());

    int32_t index = 0;
    for (const std::string &name: names) {
        const json::Value *property = properties.get(name);
        if (property == nullptr || !property->isObject())
            continue;

        ActorPropertyDescription descriptor;
        descriptor.mName = name;
        descriptor.mIndex = index++;

        const std::string type = property->get("type") != nullptr ? property->get("type")->string() : "int";
        const json::Value *range = property->get("range");
        const json::Value *defaultValue = property->get("default");
        descriptor.mClientSync = property->get("client_sync") != nullptr
                                 && property->get("client_sync")->boolean(false);

        if (type == "bool") {
            descriptor.mType = ActorPropertyDescription::Type::Bool;
            descriptor.mDefaultBool = defaultValue != nullptr && defaultValue->boolean(false);
            descriptor.mDefaultInt = descriptor.mDefaultBool ? 1 : 0;
            descriptor.mMinInt = 0;
            descriptor.mMaxInt = 1;
            if (isExpression(defaultValue))
                descriptor.mDefaultExpression = defaultValue->mString;
        } else if (type == "float") {
            descriptor.mType = ActorPropertyDescription::Type::Float;
            if (range != nullptr && range->isArray() && range->mArray.size() >= 2) {
                descriptor.mMinFloat = (float) range->mArray[0]->number(0.0);
                descriptor.mMaxFloat = (float) range->mArray[1]->number(0.0);
            }
            descriptor.mDefaultFloat = defaultValue != nullptr ? (float) defaultValue->number(0.0) : 0.0f;
            if (isExpression(defaultValue))
                descriptor.mDefaultExpression = defaultValue->mString;
        } else if (type == "enum") {
            descriptor.mType = ActorPropertyDescription::Type::Enum;
            const json::Value *values = property->get("values");
            if (values != nullptr && values->isArray()) {
                for (const std::unique_ptr<json::Value> &value: values->mArray) {
                    if (value->isString())
                        descriptor.mEnumValues.push_back(value->string());
                }
            }
            descriptor.mMinInt = 0;
            descriptor.mMaxInt = (int32_t) descriptor.mEnumValues.size() - 1;
            if (defaultValue != nullptr && defaultValue->isString())
                descriptor.mDefaultInt = std::max(0, descriptor.findEnumIndex(defaultValue->string()));
        } else {
            descriptor.mType = ActorPropertyDescription::Type::Int;
            if (range != nullptr && range->isArray() && range->mArray.size() >= 2) {
                descriptor.mMinInt = range->mArray[0]->integer(0);
                descriptor.mMaxInt = range->mArray[1]->integer(0);
            }
            descriptor.mDefaultInt = defaultValue != nullptr ? defaultValue->integer(0) : 0;
            if (isExpression(defaultValue))
                descriptor.mDefaultExpression = defaultValue->mString;
        }

        schema.push_back(descriptor);
    }

    return schema;
}

const ActorPropertyDescription *ActorPropertySchema::find(const std::vector<ActorPropertyDescription> &schema,
                                                          const std::string &name) {
    for (const ActorPropertyDescription &property: schema) {
        if (property.mName == name)
            return &property;
    }
    return nullptr;
}
