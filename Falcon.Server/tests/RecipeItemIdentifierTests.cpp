#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/FurnaceBlockActor.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Inventory/CraftingManager.h"
#include "World/Item/CraftingRecipeTable.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    const Vector3i FURNACE_POSITION(8, 100, 8);

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    bool hasMetadataSuffix(const char *identifier, int32_t &metadata) {
        const std::string itemId(identifier == nullptr ? "" : identifier);
        const size_t separator = itemId.find_last_of(':');
        if (separator == std::string::npos || separator + 1 == itemId.size())
            return false;
        for (size_t index = separator + 1; index < itemId.size(); ++index) {
            if (itemId[index] < '0' || itemId[index] > '9')
                return false;
        }
        metadata = std::stoi(itemId.substr(separator + 1));
        return true;
    }

    void requireCanonicalIdentifier(const char *identifier, const char *kind) {
        int32_t metadata = 0;
        if (hasMetadataSuffix(identifier, metadata))
            throw std::runtime_error(std::string(kind) + " ingredient kept metadata in its item id: " + identifier);
    }

    void recipeIdentifiers() {
        const CraftingRecipeData *craftingRecipes = CraftingRecipeTable::getRecipes();
        const CraftingIngredientData *ingredients = CraftingRecipeTable::getIngredients();
        for (size_t recipeIndex = 0; recipeIndex < CraftingRecipeTable::getRecipeCount(); ++recipeIndex) {
            const CraftingRecipeData &recipe = craftingRecipes[recipeIndex];
            for (uint32_t index = 0; index < recipe.mIngredientCount; ++index) {
                const CraftingIngredientData &ingredient = ingredients[recipe.mIngredientOffset + index];
                requireCanonicalIdentifier(ingredient.mItemId, "crafting");
            }
        }

        const FurnaceRecipeData *furnaceRecipes = CraftingRecipeTable::getFurnaceRecipes();
        for (size_t index = 0; index < CraftingRecipeTable::getFurnaceRecipeCount(); ++index) {
            requireCanonicalIdentifier(furnaceRecipes[index].mInputItemId, "furnace");
        }

        const StonecutterRecipeData *stonecutterRecipes = CraftingRecipeTable::getStonecutterRecipes();
        for (size_t index = 0; index < CraftingRecipeTable::getStonecutterRecipeCount(); ++index) {
            requireCanonicalIdentifier(stonecutterRecipes[index].mInputItemId, "stonecutter");
        }
    }

    void bedMetadataCrafts() {
        ServerNetworkHandler owner("Recipe metadata", "", 1);
        const CraftingRecipeData *recipes = CraftingRecipeTable::getRecipes();
        const CraftingRecipeData *bedDyeRecipe = nullptr;
        for (size_t index = 0; index < CraftingRecipeTable::getRecipeCount(); ++index) {
            if (std::string(recipes[index].mRecipeId) == "bed_dye_0_1") {
                bedDyeRecipe = &recipes[index];
                break;
            }
        }
        require(bedDyeRecipe != nullptr, "the metadata-specific bed dye recipe is missing");

        std::vector<ItemStack> grid(4, ItemStack::air());
        grid[0] = owner.createItemStack("minecraft:bed", 1);
        grid[0].mDamage = 14;
        grid[1] = owner.createItemStack("minecraft:ink_sac", 1);
        CraftingRecipeMatch match;
        require(CraftingManager::matchRecipe(grid, 2, *bedDyeRecipe, match),
                "a bed's item metadata stopped matching its dye recipe");
    }

    void sandSmelts() {
        ServerNetworkHandler owner("Recipe item id", "", 1);
        Level &level = owner.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        level.setBlock(FURNACE_POSITION, VanillaBlocks::fromIdentifier("minecraft:furnace")->toBlockState(), false);
        FurnaceBlockActor &furnace = level.getBlockActors().getOrCreate<FurnaceBlockActor>(FURNACE_POSITION);
        furnace.mInventory.setContainerItem(0, owner.createItemStack("minecraft:sand", 1));
        furnace.mInventory.setContainerItem(1, owner.createItemStack("minecraft:coal", 1));

        for (int tick = 0; tick < 200; ++tick)
            furnace.tick(owner);

        const ItemStack &output = furnace.getInventory().getContainerItem(FurnaceInventory::SLOT_OUTPUT);
        require(!output.isAir() && output.mDefinition != nullptr
                        && output.mDefinition->getIdentifier() == "minecraft:glass" && output.mCount == 1,
                "a furnace did not smelt sand into glass");
    }
}

int main(int argc, char **argv) {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-recipe-item-id-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        require(argc == 2, "expected identifiers, sand-smelts or bed-metadata");
        if (std::string(argv[1]) == "identifiers")
            recipeIdentifiers();
        else if (std::string(argv[1]) == "sand-smelts")
            sandSmelts();
        else if (std::string(argv[1]) == "bed-metadata")
            bedMetadataCrafts();
        else
            throw std::runtime_error("unknown test case");
    } catch (const std::exception &error) {
        std::clog << "Recipe item identifier regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
