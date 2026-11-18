#include "Network/Handler/LoginHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Level/Block/Actor/FurnaceBlockActor.h"
#include "World/Level/Block/Inventory/FurnaceInventory.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    const Vector3i AT(8, 64, 8);

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Fixture {
        ServerNetworkHandler mOwner{"Furnace recipe kinds", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Fixture() {
            mLevel.getChunk(0, 0);
        }
    };

    std::string smeltOnce(const std::string &block, FurnaceKind kind, const std::string &input) {
        Fixture fixture;
        fixture.mLevel.setBlockState(AT.x, AT.y, AT.z, BlockState(block));
        FurnaceBlockActor &furnace = fixture.mLevel.getBlockActors().getOrCreate<FurnaceBlockActor>(AT);
        furnace.mKind = kind;
        furnace.mBurnTime = 1000;
        furnace.mMaxBurnTime = 1000;
        furnace.mCookTime = FurnaceInventory::cookDuration(kind) - 1;
        furnace.mInventory.mItems[FurnaceInventory::SLOT_INPUT] = fixture.mOwner.createItemStack(input, 1);

        InventoryManager::tickStoredFurnace(fixture.mOwner, furnace);
        const ItemStack &output = furnace.mInventory.mItems[FurnaceInventory::SLOT_OUTPUT];
        return output.mDefinition == nullptr ? std::string() : output.mDefinition->getIdentifier();
    }

    bool hasRecipe(const CraftingDataPacket &packet, const std::string &block, const std::string &input) {
        const auto matches = [&](const CraftingRecipeEntry &entry) {
            if (entry.mBlockName != block)
                return false;
            for (const RecipeIngredientEntry &ingredient: entry.mInputs) {
                if (ingredient.mHasItem && ingredient.mItemId == input)
                    return true;
            }
            return false;
        };
        for (const CraftingRecipeEntry &entry: packet.mShapelessRecipes) {
            if (matches(entry))
                return true;
        }
        return false;
    }

    void checkServerSmelting() {
        require(smeltOnce("minecraft:lit_furnace", FurnaceKind::Furnace, "minecraft:raw_iron")
                        == "minecraft:iron_ingot",
                "a regular furnace did not smelt raw iron");
        require(smeltOnce("minecraft:lit_blast_furnace", FurnaceKind::BlastFurnace, "minecraft:raw_iron")
                        == "minecraft:iron_ingot",
                "a blast furnace did not smelt raw iron");
        require(smeltOnce("minecraft:lit_smoker", FurnaceKind::Smoker, "minecraft:beef")
                        == "minecraft:cooked_beef",
                "a smoker did not cook beef");
        require(smeltOnce("minecraft:lit_blast_furnace", FurnaceKind::BlastFurnace, "minecraft:beef").empty(),
                "a blast furnace cooked food");
        require(smeltOnce("minecraft:lit_smoker", FurnaceKind::Smoker, "minecraft:raw_iron").empty(),
                "a smoker smelted ore");
    }

    void checkCraftingData() {
        ServerNetworkHandler owner("Furnace recipe packets", "", 1);
        LoginHandler::buildCraftingData(owner);
        const CraftingDataPacket &packet = owner.getCachedCraftingData();
        require(hasRecipe(packet, "furnace", "minecraft:raw_iron")
                        && hasRecipe(packet, "blast_furnace", "minecraft:raw_iron")
                        && !hasRecipe(packet, "blast_furnace", "minecraft:beef"),
                "blast furnace crafting data did not match vanilla recipe kinds");
        require(hasRecipe(packet, "smoker", "minecraft:beef")
                        && !hasRecipe(packet, "smoker", "minecraft:raw_iron"),
                "smoker crafting data did not match vanilla recipe kinds");
    }
}

int main() {
    try {
        checkServerSmelting();
        checkCraftingData();
        std::cout << "furnace recipe kind checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
