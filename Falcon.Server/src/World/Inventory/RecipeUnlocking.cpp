#include "World/Inventory/RecipeUnlocking.h"

#include "Network/Handler/LoginHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/UnlockedRecipesPacket.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "World/Level/GameRules.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

namespace {
    const char *const TAG_USED_CONTEXTS = "used_contexts";
    const char *const TAG_UNLOCKED_RECIPES = "unlocked_recipes";

    using Context = RecipeUnlockRequirement::Context;
    using Action = UnlockedRecipesPacket::ActionType;

    uint32_t contextBit(Context context) {
        return 1u << (uint32_t) context;
    }

    const std::vector<std::string> NO_IDS;
    const std::string NO_RECIPE;
}

void RecipeUnlockIndex::add(const std::string &recipeId, const std::string &clientId, int32_t netId,
                            const RecipeUnlockRequirement *requirement) {
    mByNetId[netId] = recipeId;
    auto [found, created] = mRecipes.try_emplace(recipeId);
    found->second.mClientIds.push_back(clientId);
    if (!created || requirement == nullptr || !requirement->isUnlockable())
        return;

    found->second.mUnlockable = true;
    mUnlockable.push_back(recipeId);
    mByContext[(size_t) requirement->mContext].push_back(recipeId);
    for (const RecipeIngredientEntry &item: requirement->mItems) {
        if (item.mType == RecipeIngredientType::ItemTag)
            mByTag[item.mItemTag].push_back(recipeId);
        else
            mByItem[item.mItemId].push_back({recipeId, item.mAuxValue});
    }
}

bool RecipeUnlockIndex::contains(const std::string &recipeId) const {
    return mRecipes.contains(recipeId);
}

bool RecipeUnlockIndex::isUnlockable(const std::string &recipeId) const {
    const auto found = mRecipes.find(recipeId);
    return found != mRecipes.end() && found->second.mUnlockable;
}

const std::vector<std::string> &RecipeUnlockIndex::getClientIds(const std::string &recipeId) const {
    const auto found = mRecipes.find(recipeId);
    return found == mRecipes.end() ? NO_IDS : found->second.mClientIds;
}

const std::string &RecipeUnlockIndex::getRecipeOfNetId(int32_t netId) const {
    const auto found = mByNetId.find(netId);
    return found == mByNetId.end() ? NO_RECIPE : found->second;
}

void RecipeUnlockIndex::collectUnlockedBy(const ItemStack &item, std::vector<std::string> &out) const {
    if (item.isAir())
        return;

    const std::string &identifier = item.mDefinition->getIdentifier();
    const auto byItem = mByItem.find(identifier);
    if (byItem != mByItem.end()) {
        for (const ItemRequirement &requirement: byItem->second) {
            if (requirement.mAuxValue < 0 || requirement.mAuxValue == 0x7fff || requirement.mAuxValue == item.mDamage)
                out.push_back(requirement.mRecipeId);
        }
    }

    if (mByTag.empty())
        return;
    for (const std::string &tag: CraftingRecipeTable::getItemTags(identifier)) {
        const auto byTag = mByTag.find(tag);
        if (byTag != mByTag.end())
            out.insert(out.end(), byTag->second.begin(), byTag->second.end());
    }
}

void PlayerRecipeUnlocks::save(Tag &player) const {
    Tag data = Tag::ofCompound();
    data.putInt(TAG_USED_CONTEXTS, (int32_t) mUsedContexts);
    Tag recipes = Tag::ofList(Tag::Type::String);
    for (const std::string &recipe: mUnlocked)
        recipes.addToList(Tag::ofString(recipe));
    data.put(TAG_UNLOCKED_RECIPES, std::move(recipes));
    player.put(TAG, std::move(data));
}

void PlayerRecipeUnlocks::load(const Tag &player) {
    mUnlocked.clear();
    mUsedContexts = 0;
    const Tag *data = player.get(TAG);
    if (data == nullptr || !data->isCompound())
        return;

    mUsedContexts = (uint32_t) data->getInt(TAG_USED_CONTEXTS, 0);
    const Tag *recipes = data->get(TAG_UNLOCKED_RECIPES);
    if (recipes == nullptr || !recipes->isList())
        return;
    for (const Tag &recipe: recipes->getList()) {
        if (recipe.getType() == Tag::Type::String)
            mUnlocked.insert(recipe.asString());
    }
}

void RecipeUnlockSystem::_send(ServerNetworkHandler &owner, ServerPlayer &player, int32_t action,
                               const std::vector<std::string> &recipes) {
    const RecipeUnlockIndex &index = owner.getRecipeUnlockIndex();
    UnlockedRecipesPacket packet;
    packet.mAction = (Action) action;
    for (const std::string &recipe: recipes) {
        const std::vector<std::string> &clientIds = index.getClientIds(recipe);
        packet.mUnlockedRecipes.insert(packet.mUnlockedRecipes.end(), clientIds.begin(), clientIds.end());
    }
    owner.sendPacketTo(player.getNetworkIdentifier(), packet);
}

void RecipeUnlockSystem::_unlockContext(ServerPlayer &player, const RecipeUnlockIndex &index, Context context,
                                        std::vector<std::string> &unlocked) {
    PlayerRecipeUnlocks &unlocks = player.getRecipeUnlocks();
    for (const std::string &recipe: index.getRecipes(context)) {
        if (unlocks.unlock(recipe))
            unlocked.push_back(recipe);
    }
    unlocks.mUsedContexts |= contextBit(context);
}

// the game's order: the always-unlocked recipes once a session, then water, then a crowded inventory, then
// the items that just came in
void RecipeUnlockSystem::tick(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (!player.isSpawned() || player.isSpectator())
        return;

    const RecipeUnlockIndex &index = owner.getRecipeUnlockIndex();
    PlayerRecipeUnlocks &unlocks = player.getRecipeUnlocks();
    if (!unlocks.mSent) {
        std::vector<std::string> ignored;
        _unlockContext(player, index, Context::AlwaysUnlocked, ignored);
        if (unlocks.getUnlockedCount() != 0) {
            const std::vector<std::string> all(unlocks.getUnlocked().begin(), unlocks.getUnlocked().end());
            _send(owner, player, (int32_t) Action::InitiallyUnlocked, all);
        }
        unlocks.mSent = true;
    }

    // the slots are read every tick, so an item seen while everything was unlocked is not seen as new later
    PlayerInventory &inventory = player.getInventory();
    std::array<bool, PlayerInventory::CONTAINER_SIZE> changed{};
    bool anyChanged = false;
    int32_t stacks = 0;
    for (int slot = 0; slot < PlayerInventory::CONTAINER_SIZE; ++slot) {
        const ItemStack &item = inventory.getItem(slot);
        const std::pair<const ItemDefinition *, int32_t> seen(item.isAir() ? nullptr : item.mDefinition.get(),
                                                              item.isAir() ? 0 : item.mDamage);
        if (seen.first != nullptr)
            ++stacks;
        if (unlocks.mSeenSlots[(size_t) slot] == seen)
            continue;
        unlocks.mSeenSlots[(size_t) slot] = seen;
        changed[(size_t) slot] = seen.first != nullptr;
        anyChanged = true;
    }

    if (unlocks.getUnlockedCount() >= index.getUnlockable().size())
        return;
    // with limited crafting, recipes only come from the command
    if (owner.getWorldFor(player).getOverworld().getGameRules().getBool("dolimitedcrafting"))
        return;

    std::vector<std::string> unlocked;
    if ((unlocks.mUsedContexts & contextBit(Context::PlayerInWater)) == 0
        && LiquidBlocksFetch::at(owner.getLevelFor(player), player.getPosition()).water)
        _unlockContext(player, index, Context::PlayerInWater, unlocked);

    if (anyChanged) {
        if ((unlocks.mUsedContexts & contextBit(Context::PlayerHasManyItems)) == 0 && stacks >= MANY_ITEMS)
            _unlockContext(player, index, Context::PlayerHasManyItems, unlocked);

        std::vector<std::string> candidates;
        for (int slot = 0; slot < PlayerInventory::CONTAINER_SIZE; ++slot) {
            if (changed[(size_t) slot])
                index.collectUnlockedBy(inventory.getItem(slot), candidates);
        }
        for (const std::string &recipe: candidates) {
            if (unlocks.unlock(recipe))
                unlocked.push_back(recipe);
        }
    }

    if (!unlocked.empty())
        _send(owner, player, (int32_t) Action::NewlyUnlocked, unlocked);
}

bool RecipeUnlockSystem::give(ServerNetworkHandler &owner, ServerPlayer &player, const std::string &recipeId) {
    if (!player.getRecipeUnlocks().unlock(recipeId))
        return false;
    _send(owner, player, (int32_t) Action::NewlyUnlocked, {recipeId});
    return true;
}

bool RecipeUnlockSystem::giveAll(ServerNetworkHandler &owner, ServerPlayer &player) {
    std::vector<std::string> unlocked;
    for (const std::string &recipe: owner.getRecipeUnlockIndex().getUnlockable()) {
        if (player.getRecipeUnlocks().unlock(recipe))
            unlocked.push_back(recipe);
    }
    if (unlocked.empty())
        return false;
    _send(owner, player, (int32_t) Action::NewlyUnlocked, unlocked);
    return true;
}

bool RecipeUnlockSystem::take(ServerNetworkHandler &owner, ServerPlayer &player, const std::string &recipeId) {
    if (!player.getRecipeUnlocks().lock(recipeId))
        return false;
    _send(owner, player, (int32_t) Action::RemoveUnlocked, {recipeId});
    _afterTake(owner, player);
    return true;
}

bool RecipeUnlockSystem::takeAll(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (player.getRecipeUnlocks().getUnlockedCount() == 0)
        return false;
    player.getRecipeUnlocks().lockAll();
    _send(owner, player, (int32_t) Action::RemoveAll, {});
    _afterTake(owner, player);
    return true;
}

// the game forgets which contexts it granted and hands the always-unlocked recipes back at once
void RecipeUnlockSystem::_afterTake(ServerNetworkHandler &owner, ServerPlayer &player) {
    PlayerRecipeUnlocks &unlocks = player.getRecipeUnlocks();
    unlocks.mUsedContexts = 0;
    std::vector<std::string> restored;
    _unlockContext(player, owner.getRecipeUnlockIndex(), Context::AlwaysUnlocked, restored);
    if (!restored.empty())
        _send(owner, player, (int32_t) Action::InitiallyUnlocked, restored);
}

bool RecipeUnlockSystem::mayCraft(ServerNetworkHandler &owner, ServerPlayer &player, int32_t recipeNetId) {
    if (!owner.getWorldFor(player).getOverworld().getGameRules().getBool("dolimitedcrafting"))
        return true;
    const RecipeUnlockIndex &index = owner.getRecipeUnlockIndex();
    const std::string &recipe = index.getRecipeOfNetId(recipeNetId);
    return recipe.empty() || !index.isUnlockable(recipe) || player.getRecipeUnlocks().isUnlocked(recipe);
}
