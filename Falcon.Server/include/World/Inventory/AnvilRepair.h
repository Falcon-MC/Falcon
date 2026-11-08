#pragma once

#include "World/Item/ItemEnchantments.h"
#include "Protocol/Types/ItemStack.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct AnvilResult {
    ItemStack mResult = ItemStack::air();
    int32_t mCost = 0;
    int32_t mMaterialCost = 0;
};

// AnvilContainerManagerModel::_createResult (sub_14CC96B10): repair by material or second item, enchantment
// merging, renaming, cumulative RepairCost penalty, "Too Expensive" limit
class AnvilRepair {
public:
    static const int32_t TOO_EXPENSIVE_COST = 40;
    static const char *TAG_REPAIR_COST;

    // newName nullptr: keep the name; enchantedBook: what a plain book becomes once enchanted
    static AnvilResult createResult(const ItemStack &input, const ItemStack &material, const std::string *newName,
                                    bool creative, const std::shared_ptr<ItemDefinition> &enchantedBook);

    static int32_t getRepairCost(const ItemStack &item);

    static bool hasCustomName(const ItemStack &item);

    // ItemEnchants::getTotalValue (sub_14555B500)
    static int32_t getEnchantValue(const std::vector<EnchantmentInstance> &enchantments, bool bookModifier);

    // sub_1436ADD20: returns the level cost the material's enchantments add
    static int32_t mergeEnchantments(const ItemStack &input, const ItemStack &material, ItemStack &result,
                                     bool bookModifier, const std::shared_ptr<ItemDefinition> &enchantedBook);

    // Item::isValidRepairItem for vanilla items repaired by a material
    static bool isValidRepairItem(const ItemStack &item, const ItemStack &material);
};
