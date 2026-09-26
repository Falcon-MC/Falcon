#include "Block/Actor/ContainerBlockActor.h"

#include "Inventory/ItemStackNbt.h"
#include "Item/CraftingRecipeTable.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/ItemDefinition.h"

#include <memory>
#include <random>
#include <string>
#include <utility>

namespace {
    const char *SOUL_CAMPFIRE = "minecraft:soul_campfire";
    const char *EXTINGUISHED_STATE = "extinguished";

    std::string identifierOf(const ItemStack &item) {
        if (item.isAir() || item.mDefinition == nullptr)
            return std::string();

        return std::string(item.mDefinition->getIdentifier());
    }

    std::string slotKey(const char *prefix, int slot) {
        return std::string(prefix) + std::to_string(slot + 1);
    }

    std::mt19937 &campfireRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    Vector3f dropPositionOf(const Vector3i &position) {
        std::uniform_real_distribution<float> offset(0.0f, 1.0f);
        return Vector3f((float) position.x + offset(campfireRandom()), (float) position.y + 0.5f,
                        (float) position.z + offset(campfireRandom()));
    }
}

Tag CampfireBlockActor::saveNbt() const {
    Tag data = ContainerBlockActor::saveNbt();
    for (int slot = 0; slot < SIZE; ++slot)
        data.putInt(slotKey("ItemTime", slot), mCookTimes[slot]);
    return data;
}

void CampfireBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    ContainerBlockActor::loadNbt(data, context);
    for (int slot = 0; slot < SIZE; ++slot)
        mCookTimes[slot] = data.getInt(slotKey("ItemTime", slot));
}

Tag CampfireBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    for (int slot = 0; slot < SIZE; ++slot) {
        const ItemStack &item = mInventory.getContainerItem(slot);
        if (!item.isAir() && item.mCount > 0)
            data.put(slotKey("Item", slot), ItemStackNbt::write(item));
    }
    return data;
}

bool CampfireBlockActor::addFood(const ItemStack &item) {
    const std::string identifier = identifierOf(item);
    if (identifier.empty() || CraftingRecipeTable::findCampfireRecipe(identifier, item.mDamage, _isSoul()) == nullptr)
        return false;

    for (int slot = 0; slot < SIZE; ++slot) {
        if (!mInventory.getContainerItem(slot).isAir())
            continue;

        ItemStack food = item;
        food.mCount = 1;
        mInventory.setContainerItem(slot, std::move(food));
        mCookTimes[slot] = COOK_TIME;
        return true;
    }
    return false;
}

bool CampfireBlockActor::tick(ServerNetworkHandler &owner) {
    if (mLevel == nullptr)
        return true;

    const BlockState *state = mLevel->peekBlockPtr(mPosition.x, mPosition.y, mPosition.z);
    if (state == nullptr)
        return true;

    const bool lit = state->mStates.getByte(EXTINGUISHED_STATE) == 0;
    bool changed = false;
    for (int slot = 0; slot < SIZE; ++slot) {
        const ItemStack item = mInventory.getContainerItem(slot);
        const std::string identifier = identifierOf(item);
        if (identifier.empty()) {
            mCookTimes[slot] = 0;
            continue;
        }

        const FurnaceRecipeData *recipe = CraftingRecipeTable::findCampfireRecipe(identifier, item.mDamage,
                                                                                 _isSoul());
        if (recipe == nullptr) {
            mInventory.setContainerItem(slot, ItemStack::air());
            mCookTimes[slot] = 0;
            ItemActorHandler::dropItem(owner, *mLevel, dropPositionOf(mPosition), item,
                                       ItemActorHandler::randomDropMotion(), ItemActorHandler::DROP_PICKUP_DELAY);
            changed = true;
            continue;
        }

        if (mCookTimes[slot] > 0) {
            if (lit)
                --mCookTimes[slot];
            else
                mCookTimes[slot] = COOK_TIME;
            continue;
        }

        std::shared_ptr<ItemDefinition> output = owner.getItemDefinitions().getDefinition(recipe->mOutputItemId);
        mInventory.setContainerItem(slot, ItemStack::air());
        mCookTimes[slot] = 0;
        changed = true;
        if (output == nullptr)
            continue;

        ItemStack cooked = ItemStack::air();
        cooked.mDefinition = std::move(output);
        cooked.mBlockDefinition = owner.getBlockDefinitions().getDefinition(recipe->mOutputItemId);
        cooked.mDamage = 0;
        cooked.mCount = item.mCount;
        ItemActorHandler::dropItem(owner, *mLevel, dropPositionOf(mPosition), cooked,
                                   ItemActorHandler::randomDropMotion(), ItemActorHandler::DROP_PICKUP_DELAY);
    }

    if (changed)
        BlockActionHandler::broadcastBlockActorData(owner, *mLevel, *this);
    return true;
}

bool CampfireBlockActor::_isSoul() const {
    if (mLevel == nullptr)
        return false;

    const BlockState *state = mLevel->peekBlockPtr(mPosition.x, mPosition.y, mPosition.z);
    return state != nullptr && state->mName == SOUL_CAMPFIRE;
}
