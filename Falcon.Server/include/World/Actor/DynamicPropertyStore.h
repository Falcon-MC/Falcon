#pragma once

#include "World/Actor/DynamicPropertyValue.h"
#include "Core/NBT/Tag.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

inline Tag serializeDynamicProperties(const std::unordered_map<std::string, DynamicPropertyValue> &properties) {
    Tag root = Tag::ofCompound();

    for (const auto &entry: properties) {
        const DynamicPropertyValue &value = entry.second;
        Tag stored = Tag::ofCompound();
        stored.putByte("type", (int8_t) value.mType);

        switch (value.mType) {
            case DynamicPropertyValue::Type::Boolean:
                stored.putBool("v", value.mBoolean);
                break;
            case DynamicPropertyValue::Type::Number:
                stored.putDouble("v", value.mNumber);
                break;
            case DynamicPropertyValue::Type::String:
                stored.putString("v", value.mString);
                break;
            case DynamicPropertyValue::Type::Vector: {
                Tag vector = Tag::ofList(Tag::Type::Float);
                vector.addToList(Tag::ofFloat(value.mVector[0]));
                vector.addToList(Tag::ofFloat(value.mVector[1]));
                vector.addToList(Tag::ofFloat(value.mVector[2]));
                stored.put("v", vector);
                break;
            }
            default:
                continue;
        }

        // map keys are unique: put() would look each one up, seconds of autosave with tens of thousands
        root.append(entry.first, std::move(stored));
    }

    return root;
}

inline void deserializeDynamicProperties(const Tag &root,
                                         std::unordered_map<std::string, DynamicPropertyValue> &properties) {
    if (!root.isCompound())
        return;

    // side by side: a lookup per key is quadratic on a large compound
    const std::vector<std::string> &names = root.getKeys();
    const std::vector<Tag> &values = root.getValues();
    properties.reserve(properties.size() + names.size());
    for (size_t i = 0; i < names.size() && i < values.size(); i++) {
        const std::string &name = names[i];
        const Tag *stored = &values[i];
        if (!stored->isCompound())
            continue;

        DynamicPropertyValue value;
        value.mType = (DynamicPropertyValue::Type) stored->getByte("type", 0);

        switch (value.mType) {
            case DynamicPropertyValue::Type::Boolean:
                value.mBoolean = stored->getBool("v", false);
                break;
            case DynamicPropertyValue::Type::Number:
                value.mNumber = stored->getDouble("v", 0.0);
                break;
            case DynamicPropertyValue::Type::String:
                value.mString = stored->getString("v", std::string());
                break;
            case DynamicPropertyValue::Type::Vector: {
                const Tag *vector = stored->get("v");
                if (vector != nullptr && vector->isList() && vector->getList().size() >= 3) {
                    value.mVector[0] = vector->getList()[0].asFloat();
                    value.mVector[1] = vector->getList()[1].asFloat();
                    value.mVector[2] = vector->getList()[2].asFloat();
                }
                break;
            }
            default:
                continue;
        }

        properties[name] = value;
    }
}
