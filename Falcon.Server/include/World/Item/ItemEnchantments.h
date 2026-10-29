#pragma once

#include "World/Item/EnchantmentData.h"
#include "Protocol/Types/ItemStack.h"

#include <cstdint>
#include <string>
#include <vector>

struct EnchantmentInstance {
    int32_t mId;
    int32_t mLevel;
};

enum class EnchantmentApplyResult : int32_t {
    Success = 0,
    IncompatibleItem = 1,
    IncompatibleEnchantment = 2,
    InvalidLevel = 3
};

class ItemEnchantments {
public:
    static const char *TAG_ENCHANTMENTS;
    static const char *TAG_ENCHANTMENT_ID;
    static const char *TAG_ENCHANTMENT_LEVEL;

    static uint32_t getItemTargets(const std::string &identifier);

    static bool canApply(const std::string &identifier, const EnchantmentData &enchantment);

    static std::vector<EnchantmentInstance> read(const ItemStack &item);

    static void write(ItemStack &item, const std::vector<EnchantmentInstance> &enchantments);

    static int32_t getLevel(const ItemStack &item, int32_t enchantmentId);

    static EnchantmentApplyResult apply(ItemStack &item, const EnchantmentData &enchantment, int32_t level,
                                        const EnchantmentData **conflict);
};
