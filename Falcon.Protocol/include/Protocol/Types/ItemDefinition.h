#pragma once

#include "Core/NBT/Tag.h"

#include <string>

class ItemDefinition {
public:
    ItemDefinition(std::string identifier, int runtimeId, bool componentBased, Tag componentData);

    const std::string &getIdentifier() const { return mIdentifier; }

    int getRuntimeId() const { return mRuntimeId; }

    bool isComponentBased() const { return mComponentBased; }

    const Tag &getComponentData() const { return mComponentData; }

private:
    std::string mIdentifier;
    int mRuntimeId;
    bool mComponentBased;
    Tag mComponentData;
};
