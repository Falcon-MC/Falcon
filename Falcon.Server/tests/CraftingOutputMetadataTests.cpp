#include "Network/ServerNetworkHandler.h"
#include "Network/Handler/LoginHandler.h"
#include "World/Inventory/CraftingManager.h"
#include "World/Item/CraftingRecipeTable.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Block/CrafterBlock.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    const std::string BLUE_BANNER_RECIPE = "minecraft:blue_banner";
    const int32_t BLUE_BANNER_DAMAGE = 4;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    ItemStack item(ServerNetworkHandler &owner, const char *identifier) {
        ItemStack result = owner.createItemStack(identifier, 1);
        require(!result.isAir(), std::string("missing recipe input ") + identifier);
        return result;
    }

    std::vector<ItemStack> blueBannerGrid(ServerNetworkHandler &owner) {
        std::vector<ItemStack> grid(9, ItemStack::air());
        for (int slot = 0; slot < 6; ++slot)
            grid[(size_t) slot] = item(owner, "minecraft:blue_wool");
        grid[7] = item(owner, "minecraft:stick");
        return grid;
    }

    int32_t blueBannerNetworkId(ServerNetworkHandler &owner) {
        owner.getRecipeOutputs();
        for (const auto &entry: owner.getVanillaRecipeIdsMutable()) {
            if (entry.second == BLUE_BANNER_RECIPE)
                return entry.first;
        }
        return 0;
    }

    void run() {
        ServerNetworkHandler owner("Crafting output metadata", "", 1);
        const std::vector<ItemStack> grid = blueBannerGrid(owner);
        const std::vector<ItemStack> &outputs = owner.getRecipeOutputs();
        const std::vector<uint32_t> &sources = owner.getRecipeSourceIndices();

        const ItemStack result = CraftingManager::findResult(grid, 3, outputs, sources);
        require(!result.isAir() && result.mDefinition != nullptr
                        && result.mDefinition->getIdentifier() == "minecraft:banner"
                        && result.mDamage == BLUE_BANNER_DAMAGE,
                "crafting a blue banner did not preserve its output metadata");

        const int32_t networkId = blueBannerNetworkId(owner);
        require(networkId > 0, "the blue banner recipe has no network id");
        const CraftingDataPacket &packet = owner.getCachedCraftingData();
        const auto recipe = std::find_if(packet.mShapedRecipes.begin(), packet.mShapedRecipes.end(),
                                         [networkId](const CraftingRecipeEntry &entry) {
                                             return entry.mRecipeNetId == networkId;
                                         });
        require(recipe != packet.mShapedRecipes.end() && recipe->mOutputs.size() == 1
                        && recipe->mOutputs.front().mMeta == BLUE_BANNER_DAMAGE,
                "the crafting packet did not send the blue banner's output metadata");

        CrafterBlockActor crafter;
        for (int slot = 0; slot < 6; ++slot)
            crafter.getInventory().setContainerItem(slot, item(owner, "minecraft:blue_wool"));
        crafter.getInventory().setContainerItem(7, item(owner, "minecraft:stick"));
        const std::vector<ItemStack> automated = CrafterBlock::craftResults(owner, crafter);
        require(automated.size() == 1 && automated.front().mDefinition != nullptr
                        && automated.front().mDefinition->getIdentifier() == "minecraft:banner"
                        && automated.front().mDamage == BLUE_BANNER_DAMAGE,
                "the crafter did not preserve the blue banner's output metadata");
    }
}

int main() {
    try {
        run();
    } catch (const std::exception &error) {
        std::clog << "Crafting output metadata regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
