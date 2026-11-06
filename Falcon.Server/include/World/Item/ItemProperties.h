#pragma once

#include "Protocol/Types/ItemStack.h"

#include <cstdint>
#include <string>

class ItemProperties {
public:
    static bool canDestroy(const ItemStack &item, const std::string &blockName);
};
