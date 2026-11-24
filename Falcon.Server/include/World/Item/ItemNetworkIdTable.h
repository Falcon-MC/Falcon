#pragma once

#include "Core/NBT/Tag.h"
#include "Protocol/Types/ItemNetworkIdMap.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

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

    // empty for anything but a slab, double slab or stairs; an older client lacking the variant shows this block
    static std::string getVariantBase(const std::string &identifier);

    // sent in the client's item registry
    static const std::vector<ItemNetworkIdEntry> &getPalette(int32_t protocol);

    // nullptr when the ids are its own; a missing item shows as its base block for a slab or stairs, else as nothing
    static std::shared_ptr<const ItemNetworkIdMap> getNetworkIds(int32_t protocol);
};
