#pragma once

#include "Protocol/Types/ItemStack.h"

#include <cstddef>
#include <string>

class ArmorProtection {
public:
    static float apply(const ItemStack *armor, size_t count, float amount, const std::string &deathMessageKey,
                       float efficiency);

    // false for the causes armor neither reduces nor wears down under
    static bool isReducedByArmor(const std::string &deathMessageKey);

    // body armor included: its points are not in the item table
    static int getPoints(const ItemStack &item);

private:
    static int _protectionFactor(int level, float modifier);
};
