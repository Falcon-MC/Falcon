#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/RecipeUnlocking.h"
#include "World/Item/CraftingRecipeTable.h"
#include "World/Level/GameRules.h"
#include "World/Level/Level.h"
#include "World/Level/WorldManager.h"

#include <iostream>
#include <stdexcept>

namespace {
    // the game's recipe ids, one per unlocking context
    const char *const WORKBENCH = "minecraft:WorkBench_recipeId";
    const char *const BOAT = "minecraft:acacia_boat";
    const char *const CHEST = "minecraft:Chest_recipeId";
    const char *const STRIPPED_WOOD = "minecraft:acacia_wood_stripped";

    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    ItemStack item(ServerNetworkHandler &server, const char *name) {
        ItemStack result = ItemStack::air();
        result.mDefinition = server.getItemDefinitions().getDefinition(name);
        require(result.mDefinition != nullptr, "missing item definition");
        result.mCount = 1;
        return result;
    }

    void checkTable(ServerNetworkHandler &server) {
        using Context = RecipeUnlockRequirement::Context;
        const RecipeUnlockRequirement *wood = CraftingRecipeTable::getUnlockRequirement(STRIPPED_WOOD);
        require(wood != nullptr && wood->mContext == Context::None && wood->mItems.size() == 1
                        && wood->mItems.front().mItemId == "minecraft:stripped_acacia_log",
                "the stripped wood recipe lost its unlocking item");
        require(CraftingRecipeTable::getUnlockRequirement(WORKBENCH)->mContext == Context::AlwaysUnlocked
                        && CraftingRecipeTable::getUnlockRequirement(BOAT)->mContext == Context::PlayerInWater
                        && CraftingRecipeTable::getUnlockRequirement(CHEST)->mContext == Context::PlayerHasManyItems,
                "wrong unlocking contexts");

        const RecipeUnlockIndex &index = server.getRecipeUnlockIndex();
        require(index.isUnlockable(STRIPPED_WOOD) && !index.getClientIds(STRIPPED_WOOD).empty(),
                "the stripped wood recipe was not indexed");

        bool sent = false;
        for (const CraftingRecipeEntry &entry: server.getCachedCraftingData().mShapelessRecipes) {
            if (entry.mRecipeId != index.getClientIds(STRIPPED_WOOD).front())
                continue;
            sent = entry.mHasUnlockingRequirement && entry.mUnlockingContext == 0 && entry.mHasUnlockingIngredients
                   && entry.mUnlockingIngredients.size() == 1;
        }
        for (const CraftingRecipeEntry &entry: server.getCachedCraftingData().mShapedRecipes) {
            if (entry.mRecipeId == index.getClientIds(STRIPPED_WOOD).front())
                sent = entry.mHasUnlockingRequirement && entry.mUnlockingContext == 0
                       && entry.mUnlockingIngredients.size() == 1;
        }
        require(sent, "the client was not sent the unlocking requirement");
    }

    void checkPlayer(ServerNetworkHandler &server, Level &level) {
        const NetworkIdentifier id;
        ServerPlayer &player = server.getPlayers().try_emplace(id, id, 4004).first->second;
        player.setName("RecipeTest");
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.moveToLevel(level);
        player.setPosition(Vector3f(4.5f, 100.0f, 4.5f));
        PlayerRecipeUnlocks &unlocks = player.getRecipeUnlocks();

        RecipeUnlockSystem::tick(server, player);
        require(unlocks.isUnlocked(WORKBENCH) && !unlocks.isUnlocked(STRIPPED_WOOD) && !unlocks.isUnlocked(BOAT)
                        && !unlocks.isUnlocked(CHEST),
                "the first tick unlocked the wrong recipes");

        player.getInventory().setItem(3, item(server, "minecraft:stripped_acacia_log"));
        RecipeUnlockSystem::tick(server, player);
        require(unlocks.isUnlocked(STRIPPED_WOOD), "holding the log did not unlock the wood");

        for (int slot = 10; slot < 19; ++slot)
            player.getInventory().setItem(slot, item(server, "minecraft:dirt"));
        RecipeUnlockSystem::tick(server, player);
        require(unlocks.isUnlocked(CHEST), "ten stacks did not unlock the chest");

        level.setBlock(Vector3i(4, 100, 4), BlockState("minecraft:water"), false);
        RecipeUnlockSystem::tick(server, player);
        require(unlocks.isUnlocked(BOAT), "water did not unlock the boat");
        level.setBlock(Vector3i(4, 100, 4), BlockState("minecraft:air"), false);

        Tag saved = Tag::ofCompound();
        unlocks.save(saved);
        PlayerRecipeUnlocks loaded;
        loaded.load(saved);
        require(loaded.isUnlocked(BOAT) && loaded.isUnlocked(STRIPPED_WOOD)
                        && loaded.getUnlockedCount() == unlocks.getUnlockedCount()
                        && loaded.mUsedContexts == unlocks.mUsedContexts,
                "the recipe book did not survive a save");

        require(RecipeUnlockSystem::takeAll(server, player) && unlocks.isUnlocked(WORKBENCH)
                        && !unlocks.isUnlocked(STRIPPED_WOOD) && unlocks.mUsedContexts == 2,
                "taking every recipe did not bring the always-unlocked ones back");
        require(RecipeUnlockSystem::give(server, player, STRIPPED_WOOD)
                        && !RecipeUnlockSystem::give(server, player, STRIPPED_WOOD),
                "giving a recipe twice changed the book twice");

        GameRules &rules = server.getWorldFor(player).getOverworld().getGameRules();
        require(rules.getBool("recipesunlock"), "recipes are not unlocked as the game plays");
        rules.setFromString("dolimitedcrafting", "true");
        const RecipeUnlockIndex &index = server.getRecipeUnlockIndex();
        int32_t lockedNetId = -1;
        int32_t openNetId = -1;
        for (const CraftingRecipeEntry &entry: server.getCachedCraftingData().mShapedRecipes) {
            if (index.getClientIds(CHEST).front() == entry.mRecipeId)
                lockedNetId = entry.mRecipeNetId;
            if (index.getClientIds(STRIPPED_WOOD).front() == entry.mRecipeId)
                openNetId = entry.mRecipeNetId;
        }
        require(lockedNetId > 0 && openNetId > 0, "the test recipes were not sent");
        require(!RecipeUnlockSystem::mayCraft(server, player, lockedNetId)
                        && RecipeUnlockSystem::mayCraft(server, player, openNetId),
                "limited crafting did not follow the recipe book");
        rules.setFromString("dolimitedcrafting", "false");
        require(RecipeUnlockSystem::mayCraft(server, player, lockedNetId), "unlimited crafting refused a recipe");
    }
}

int main() {
    try {
        ServerNetworkHandler server("RecipeUnlocking", "", 1);
        Level &level = server.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        checkTable(server);
        checkPlayer(server, level);
    } catch (const std::exception &error) {
        std::clog << "Recipe unlocking regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
