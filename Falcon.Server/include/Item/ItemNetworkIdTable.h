#pragma once

#include "Core/NBT/Tag.h"

#include <cstddef>
#include <cstdint>
#include <string>

struct ItemNetworkIdEntry {
    std::string mIdentifier;
    int32_t mNetworkId = 0;
    bool mComponentBased = false;
    int32_t mVersion = 0;
    Tag mComponents = Tag::ofCompound();
};

class ItemNetworkIdTable {
public:
    static const ItemNetworkIdEntry *getEntries();

    static size_t getCount();

    static const ItemNetworkIdEntry *find(const std::string &identifier);
};
