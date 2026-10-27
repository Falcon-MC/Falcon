#pragma once

#include "Protocol/Types/ItemStack.h"

#include <cstddef>
#include <string>

class ArmorProtection {
public:
    static float apply(const ItemStack *armor, size_t count, float amount, const std::string &deathMessageKey,
                       float efficiency);

private:
    static int _protectionFactor(int level, float modifier);
};
