#pragma once

#include "Core/NBT/Tag.h"
#include "World/Item/CraftingRecipeTable.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ItemDefinition;
class ItemStack;
class ServerNetworkHandler;
class ServerPlayer;

// the recipes sent to clients, by the id the recipe book uses: the game's recipe id or a pack recipe's identifier.
// Ingredient variants of one recipe share its id and are unlocked together
class RecipeUnlockIndex {
public:
    void add(const std::string &recipeId, const std::string &clientId, int32_t netId,
             const RecipeUnlockRequirement *requirement);

    bool contains(const std::string &recipeId) const;

    bool isUnlockable(const std::string &recipeId) const;

    // the ids the client knows the recipe's variants by; empty for an unknown recipe
    const std::vector<std::string> &getClientIds(const std::string &recipeId) const;

    // the recipe a crafting request names; empty for one the recipe book does not track
    const std::string &getRecipeOfNetId(int32_t netId) const;

    const std::vector<std::string> &getUnlockable() const {
        return mUnlockable;
    }

    const std::vector<std::string> &getRecipes(RecipeUnlockRequirement::Context context) const {
        return mByContext[(size_t) context];
    }

    // the recipes an item unlocks by being held
    void collectUnlockedBy(const ItemStack &item, std::vector<std::string> &out) const;

private:
    struct Entry {
        bool mUnlockable = false;
        std::vector<std::string> mClientIds;
    };

    struct ItemRequirement {
        std::string mRecipeId;
        int32_t mAuxValue = -1;
    };

    std::unordered_map<std::string, Entry> mRecipes;
    std::unordered_map<int32_t, std::string> mByNetId;
    std::vector<std::string> mUnlockable;
    std::array<std::vector<std::string>, 4> mByContext;
    std::unordered_map<std::string, std::vector<ItemRequirement>> mByItem;
    std::unordered_map<std::string, std::vector<std::string>> mByTag;
};

// one player's recipe book, saved with the player
class PlayerRecipeUnlocks {
public:
    static constexpr const char *TAG = "recipe_unlocking";

    bool isUnlocked(const std::string &recipeId) const {
        return mUnlocked.count(recipeId) != 0;
    }

    bool unlock(const std::string &recipeId) {
        return mUnlocked.insert(recipeId).second;
    }

    bool lock(const std::string &recipeId) {
        return mUnlocked.erase(recipeId) != 0;
    }

    void lockAll() {
        mUnlocked.clear();
    }

    size_t getUnlockedCount() const {
        return mUnlocked.size();
    }

    const std::unordered_set<std::string> &getUnlocked() const {
        return mUnlocked;
    }

    void save(Tag &player) const;

    void load(const Tag &player);

    // a bit per unlocking context already granted: the in-water and many-items unlocks happen once
    uint32_t mUsedContexts = 0;
    // the always-unlocked recipes go out with the first tick of a session
    bool mSent = false;
    // unlocked recipes this server still has: a removed pack leaves its saved ids behind
    size_t mKnownUnlocked = 0;
    // what each inventory slot held last tick: only a new item unlocks recipes
    std::array<std::pair<const ItemDefinition *, int32_t>, 36> mSeenSlots{};

private:
    std::unordered_set<std::string> mUnlocked;
};

class RecipeUnlockSystem {
public:
    // the inventory holds this many stacks: the many-items recipes unlock
    static constexpr int32_t MANY_ITEMS = 10;

    static void tick(ServerNetworkHandler &owner, ServerPlayer &player);

    // false: the player already had it, or had none of them
    static bool give(ServerNetworkHandler &owner, ServerPlayer &player, const std::string &recipeId);

    static bool take(ServerNetworkHandler &owner, ServerPlayer &player, const std::string &recipeId);

    static bool giveAll(ServerNetworkHandler &owner, ServerPlayer &player);

    static bool takeAll(ServerNetworkHandler &owner, ServerPlayer &player);

    // with doLimitedCrafting, only unlocked recipes craft
    static bool mayCraft(ServerNetworkHandler &owner, ServerPlayer &player, int32_t recipeNetId);

private:
    static void _send(ServerNetworkHandler &owner, ServerPlayer &player, int32_t action,
                      const std::vector<std::string> &recipes);

    static void _unlockContext(ServerPlayer &player, const RecipeUnlockIndex &index,
                               RecipeUnlockRequirement::Context context, std::vector<std::string> &unlocked);

    static void _afterTake(ServerNetworkHandler &owner, ServerPlayer &player);

    static bool _unlock(ServerPlayer &player, const std::string &recipeId);
};
