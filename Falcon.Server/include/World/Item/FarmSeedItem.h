#pragma once

#include "World/Item/Item.h"

#include <string>

// the planting items that are no food: a farmer composts what it holds beyond its keep
class FarmSeedItem : public Item {
public:
    explicit FarmSeedItem(const Item &base);

    static bool matches(const std::string &identifier);

    static const Tag *getSeedComponent(const ItemStack &item);
    static bool plantsOn(const Tag &seed, const std::string &block);

    bool isFarmerCompost() const override {
        return true;
    }
};
