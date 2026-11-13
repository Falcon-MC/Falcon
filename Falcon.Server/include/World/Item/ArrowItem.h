#pragma once

#include "World/Item/Item.h"

class ArrowItem final : public Item {
public:
    explicit ArrowItem(const Item &base);

    static bool matches(const std::string &identifier);

    bool dispense(const DispenseSource &source) const override;
};
