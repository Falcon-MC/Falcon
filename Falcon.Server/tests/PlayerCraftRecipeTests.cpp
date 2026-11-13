#include "Network/ServerNetworkHandler.h"
#include "World/Inventory/ItemStackRequestHandler.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Item/CraftingRecipeTable.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    ItemStack makeItem(ServerNetworkHandler &owner, const std::string &identifier, int32_t count) {
        ItemStack item;
        item.mDefinition = owner.getItemDefinitions().getDefinition(identifier);
        item.mBlockDefinition = owner.getBlockDefinitions().getDefinition(identifier);
        item.mCount = count;
        return item;
    }

    ItemStackRequestSlotData slotOf(ContainerSlotType container, int32_t slot) {
        ItemStackRequestSlotData data;
        data.mContainer = container;
        data.mContainerName.mContainer = container;
        data.mSlot = slot;
        return data;
    }

    // a shapeless one-ingredient recipe (a log into planks, say): the simplest grid craft to drive
    bool findSimpleRecipe(ServerNetworkHandler &owner, int32_t &outNetId, std::string &outIngredient,
                          std::string &outOutput, int32_t &outOutputCount) {
        const std::vector<ItemStack> &outputs = owner.getRecipeOutputs();
        const std::vector<uint32_t> &sources = owner.getRecipeSourceIndices();
        const CraftingRecipeData *recipes = CraftingRecipeTable::getRecipes();
        const CraftingIngredientData *ingredients = CraftingRecipeTable::getIngredients();
        const CraftingOutputData *recipeOutputs = CraftingRecipeTable::getOutputs();

        for (size_t k = 0; k < outputs.size() && k < sources.size(); ++k) {
            const CraftingRecipeData &source = recipes[sources[k]];
            if (source.mWidth > 0 || source.mIngredientCount != 1 || source.mOutputCount == 0)
                continue;
            const CraftingIngredientData &ingredient = ingredients[source.mIngredientOffset];
            if (ingredient.mItemId == nullptr || ingredient.mCount != 1)
                continue;
            const CraftingOutputData &output = recipeOutputs[source.mOutputOffset];
            if (owner.getItemDefinitions().getDefinition(ingredient.mItemId) == nullptr
                || owner.getItemDefinitions().getDefinition(output.mItemId) == nullptr)
                continue;

            outNetId = (int32_t) k + 1;
            outIngredient = ingredient.mItemId;
            outOutput = output.mItemId;
            outOutputCount = output.mCount;
            return true;
        }
        return false;
    }

    void run(ServerNetworkHandler &owner) {
        int32_t netId = 0;
        int32_t outputCount = 0;
        std::string ingredient;
        std::string output;
        require(findSimpleRecipe(owner, netId, ingredient, output, outputCount),
                "no one-ingredient shapeless crafting recipe was loaded");

        PlayerInventory inventory;
        // the 2x2 grid, local slot 0 = network slot 28
        inventory.setCraftingItem(0, makeItem(owner, ingredient, 1));

        ItemStackRequest request;
        request.mRequestId = -1;

        ItemStackRequestAction craft;
        craft.mType = ItemStackRequestActionType::CraftRecipe;
        craft.mRecipeNetworkId = netId;
        craft.mNumberOfRequestedCrafts = 1;
        request.mActions.push_back(craft);

        ItemStackRequestAction consume;
        consume.mType = ItemStackRequestActionType::Consume;
        consume.mCount = 1;
        consume.mSource = slotOf(ContainerSlotType::CraftingInput, PlayerInventory::CRAFTING_NETWORK_SLOT_FIRST);
        request.mActions.push_back(consume);

        ItemStackRequestAction take;
        take.mType = ItemStackRequestActionType::Take;
        take.mCount = outputCount;
        take.mSource = slotOf(ContainerSlotType::CreatedOutput, 50);
        take.mDestination = slotOf(ContainerSlotType::Inventory, 0);
        request.mActions.push_back(take);

        const std::vector<CreativeItemData> creative;
        ItemStack crafted = ItemStack::air();
        const ItemStackResponseEntry entry = ItemStackRequestHandler::execute(
                inventory, request, creative, owner.getRecipeOutputs(), owner.getRecipeSourceIndices(), false, false,
                nullptr, nullptr, &owner.getCodecContext(), nullptr, 0, false, nullptr, false, nullptr, nullptr,
                nullptr, nullptr, nullptr, &crafted);

        require(entry.mResult == ItemStackRequestHandler::RESULT_OK, "a valid grid craft was refused");
        require(!crafted.isAir() && crafted.mDefinition != nullptr
                        && crafted.mDefinition->getIdentifier() == output,
                "the crafted item reported to scripts is not the recipe output");
        require(crafted.mCount == outputCount, "the crafted item count is not the recipe output count");

        // a request that crafts nothing leaves the crafted item untouched
        ItemStackRequest empty;
        empty.mRequestId = -2;
        ItemStack none = ItemStack::air();
        PlayerInventory spare;
        ItemStackRequestHandler::execute(spare, empty, creative, owner.getRecipeOutputs(),
                                         owner.getRecipeSourceIndices(), false, false, nullptr, nullptr,
                                         &owner.getCodecContext(), nullptr, 0, false, nullptr, false, nullptr, nullptr,
                                         nullptr, nullptr, nullptr, &none);
        require(none.isAir(), "a request with no craft still reported a crafted item");
    }
}

int main() {
    try {
        ServerNetworkHandler owner("Player craft recipe", "", 1);
        run(owner);
    } catch (const std::exception &error) {
        std::clog << "Player craft recipe regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
