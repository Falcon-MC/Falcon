#pragma once

#include "World/Item/Item.h"

#include <string>

// bread and the crops a villager takes to breed
class VillagerFoodItem : public Item {
public:
    explicit VillagerFoodItem(const Item &base);

    static bool matches(const std::string &identifier);

    int32_t getVillagerBreedingCount() const override;
};
