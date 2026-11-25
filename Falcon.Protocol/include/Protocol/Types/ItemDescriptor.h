#pragma once

#include "Protocol/Types/ItemDefinition.h"

#include <cstdint>
#include <memory>
#include <string>

enum class ItemDescriptorType {
    Invalid,
    Default,
    Molang,
    ItemTag,
};

class ItemDescriptor {
public:
    ItemDescriptorType mType = ItemDescriptorType::Invalid;

    std::shared_ptr<ItemDefinition> mItemDefinition;
    int32_t mAuxValue = 0;

    std::string mTagExpression;
    int16_t mMolangVersion = 0;

    std::string mItemTag;
};

class ItemDescriptorWithCount {
public:
    ItemDescriptor mDescriptor;
    int32_t mCount = 0;
};
