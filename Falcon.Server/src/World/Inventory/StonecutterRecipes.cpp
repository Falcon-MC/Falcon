#include "World/Inventory/StonecutterRecipes.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>

bool StonecutterRecipes::matchesInput(const StonecutterRecipeData &recipe, const ItemStack &input) {
    if (input.isAir() || input.mCount <= 0)
        return false;

    std::string itemId = recipe.mInputItemId;
    int32_t auxValue = recipe.mInputAuxValue;
    // recipes.json names some inputs "namespace:name:aux" with the aux value in the name
    const size_t namespaceEnd = itemId.find(':');
    const size_t auxStart = itemId.rfind(':');
    if (namespaceEnd != std::string::npos && auxStart != namespaceEnd && auxStart + 1 < itemId.size()
        && std::isdigit((unsigned char) itemId[auxStart + 1])) {
        auxValue = (int32_t) std::strtol(itemId.c_str() + auxStart + 1, nullptr, 10);
        itemId.resize(auxStart);
    }

    if (input.mDefinition->getIdentifier() != itemId)
        return false;

    return auxValue < 0 || input.mDamage == auxValue;
}

std::vector<const StonecutterRecipeData *> StonecutterRecipes::validRecipes(const ItemStack &input) {
    std::vector<const StonecutterRecipeData *> recipes;
    const StonecutterRecipeData *all = CraftingRecipeTable::getStonecutterRecipes();
    for (size_t index = 0; index < CraftingRecipeTable::getStonecutterRecipeCount(); ++index) {
        if (matchesInput(all[index], input))
            recipes.push_back(&all[index]);
    }

    std::sort(recipes.begin(), recipes.end(),
              [](const StonecutterRecipeData *left, const StonecutterRecipeData *right) {
                  return std::string(left->mRecipeId) < std::string(right->mRecipeId);
              });
    return recipes;
}

ItemStack StonecutterRecipes::createOutput(const StonecutterRecipeData &recipe, const PacketCodecContext &context) {
    ItemStack output = ItemStack::air();
    output.mDefinition = context.getItemDefinitions().getDefinition(recipe.mOutputItemId);
    if (output.mDefinition == nullptr)
        return ItemStack::air();

    output.mBlockDefinition = context.getBlockDefinitions().getDefinition(recipe.mOutputItemId);
    output.mCount = recipe.mOutputCount;
    return output;
}
