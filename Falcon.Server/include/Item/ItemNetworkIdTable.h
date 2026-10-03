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

    /**
     * The block a slab, double slab or stairs is made of, or an empty string for any other identifier. An older
     * client lacking such a variant shows it as this block.
     */
    static std::string getVariantBase(const std::string &identifier);

    /**
     * The item palette a client of this protocol knows, sent to it in its item registry.
     */
    static const std::vector<ItemNetworkIdEntry> &getPalette(int32_t protocol);

    /**
     * How the server's item IDs translate for a client of this protocol, or nullptr when they are its own. An item
     * the client lacks shows as its base block when it is a slab or stairs of one, and otherwise as nothing.
     */
    static std::shared_ptr<const ItemNetworkIdMap> getNetworkIds(int32_t protocol);
};
