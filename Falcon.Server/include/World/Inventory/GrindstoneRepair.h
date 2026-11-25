#pragma once

#include "Protocol/Types/ItemStack.h"

struct GrindstoneResult {
    ItemStack mItem = ItemStack::air();
    int32_t mExperienceCost = 0;
};

namespace GrindstoneRepair {
    GrindstoneResult createResult(const ItemStack &input, const ItemStack &additional,
                                  const std::shared_ptr<ItemDefinition> &book);
}
