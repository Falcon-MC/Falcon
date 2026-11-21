#pragma once

#include "World/Item/Item.h"

#include <string>

// the planting items that are no food: a farmer composts what it holds beyond its keep
class FarmSeedItem : public Item {
public:
    explicit FarmSeedItem(const Item &base);

    static bool matches(const std::string &identifier);

    bool isFarmerCompost() const override {
        return true;
    }
};
