#include "Network/ServerNetworkHandler.h"
#include "World/Item/CraftingRecipeTable.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    const StonecutterRecipeData *findRecipe(const char *recipeId) {
        const StonecutterRecipeData *recipes = CraftingRecipeTable::getStonecutterRecipes();
        for (size_t index = 0; index < CraftingRecipeTable::getStonecutterRecipeCount(); ++index) {
            if (std::strcmp(recipes[index].mRecipeId, recipeId) == 0)
                return &recipes[index];
        }
        return nullptr;
    }

    void checkRecipeTable() {
        require(CraftingRecipeTable::getStonecutterRecipeCount() > 0, "no stonecutter recipe was loaded");
        const StonecutterRecipeData *slab = findRecipe("minecraft:stonecutter_andesite_slab");
        require(slab != nullptr, "the andesite slab stonecutter recipe is missing");
        require(std::string(slab->mInputItemId) == "minecraft:andesite" && slab->mInputAuxValue == -1,
                "the andesite slab recipe reads the wrong input");
        require(std::string(slab->mOutputItemId) == "minecraft:andesite_slab" && slab->mOutputCount == 2,
                "the andesite slab recipe reads the wrong output");
        require(std::string(slab->mUuid) == "82f4e0ca-2e60-46ac-84e8-0dc309bfdb07",
                "the andesite slab recipe reads the wrong uuid");
    }

    void checkCraftingData() {
        ServerNetworkHandler owner("Stonecutter recipes", "", 1);
        const std::unordered_map<int32_t, uint32_t> &indices = owner.getStonecutterRecipeIndices();
        require(!indices.empty(), "no stonecutter recipe reached the crafting data");

        const CraftingDataPacket &packet = owner.getCachedCraftingData();
        int32_t lastOther = 0;
        int32_t firstStonecutter = INT32_MAX;
        size_t stonecutterCount = 0;
        auto visit = [&](const CraftingRecipeEntry &entry) {
            if (entry.mBlockName != "stonecutter") {
                lastOther = std::max(lastOther, entry.mRecipeNetId);
                return;
            }
            ++stonecutterCount;
            firstStonecutter = std::min(firstStonecutter, entry.mRecipeNetId);
            require(indices.count(entry.mRecipeNetId) == 1, "a stonecutter recipe has no table index");
            require(entry.mWidth == 0 && entry.mInputs.size() == 1 && entry.mOutputs.size() == 1,
                    "a stonecutter recipe is not a one-input shapeless recipe");
        };
        for (const CraftingRecipeEntry &entry: packet.mShapedRecipes)
            visit(entry);
        for (const CraftingRecipeEntry &entry: packet.mShapelessRecipes)
            visit(entry);
        for (const MultiRecipeEntry &entry: packet.mMultiRecipes)
            lastOther = std::max(lastOther, entry.mRecipeNetId);

        require(stonecutterCount == indices.size(), "stonecutter recipes and their indices disagree");
        require(firstStonecutter == lastOther + 1, "stonecutter recipes do not follow every other recipe");

        const auto andesite = std::find_if(
                packet.mShapelessRecipes.begin(), packet.mShapelessRecipes.end(),
                [&](const CraftingRecipeEntry &entry) {
                    const auto it = indices.find(entry.mRecipeNetId);
                    return entry.mBlockName == "stonecutter" && it != indices.end()
                           && std::string(CraftingRecipeTable::getStonecutterRecipes()[it->second].mRecipeId)
                                      == "minecraft:stonecutter_andesite_slab";
                });
        require(andesite != packet.mShapelessRecipes.end(), "the andesite slab recipe is not sent");
        require(andesite->mInputs.front().mItemId == "minecraft:andesite"
                        && andesite->mInputs.front().mAuxValue == 0x7fff && andesite->mOutputs.front().mCount == 2,
                "the andesite slab recipe is sent wrong");
    }
}

int main() {
    try {
        checkRecipeTable();
        checkCraftingData();
    } catch (const std::exception &error) {
        std::clog << "Stonecutter regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
