#pragma once

#include "Protocol/Types/ItemStack.h"

#include <string>

class ItemCooldowns {
public:
    static int getDuration(const ItemStack &item);
    static int getDuration(const std::string &identifier);
    static std::string getCategory(const ItemStack &item);
    static std::string getCategory(const std::string &identifier);
    static bool isSupported(const ItemStack &item);
};
