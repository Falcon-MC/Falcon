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

/**
 * Computes what an anvil makes from an input, a material and a requested name, following
 * AnvilContainerManagerModel::_createResult (sub_14CC96B10): repair by material or by a second item,
 * enchantment merging, renaming, the cumulative RepairCost penalty and the "Too Expensive" limit.
 */
class AnvilRepair {
public:
    static const int32_t TOO_EXPENSIVE_COST = 40;
    static const char *TAG_REPAIR_COST;

    /**
     * newName is the name typed in the anvil, or nullptr when the request leaves the name as it is.
     * enchantedBook turns a plain book into an enchanted book once it receives enchantments.
     */
    static AnvilResult createResult(const ItemStack &input, const ItemStack &material, const std::string *newName,
                                    bool creative, const std::shared_ptr<ItemDefinition> &enchantedBook);

    static int32_t getRepairCost(const ItemStack &item);

    static bool hasCustomName(const ItemStack &item);

    /** ItemEnchants::getTotalValue (sub_14555B500). */
    static int32_t getEnchantValue(const std::vector<EnchantmentInstance> &enchantments, bool bookModifier);

    /**
     * Applies the material's enchantments to result and returns the level cost they add, as
     * sub_1436ADD20 does.
     */
    static int32_t mergeEnchantments(const ItemStack &input, const ItemStack &material, ItemStack &result,
                                     bool bookModifier, const std::shared_ptr<ItemDefinition> &enchantedBook);

    /** Item::isValidRepairItem for the vanilla items repaired by a material. */
    static bool isValidRepairItem(const ItemStack &item, const ItemStack &material);
};
