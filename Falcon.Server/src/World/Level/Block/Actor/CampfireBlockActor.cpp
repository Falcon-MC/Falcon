#include "World/Level/Block/Actor/ContainerBlockActor.h"

#include "Falcon/Plugin/PluginEvent.h"
#include "Falcon/Plugin/PluginManager.h"
#include "World/Level/Block/ContainerBlock.h"
#include "World/Inventory/ItemStackNbt.h"
#include "World/Item/CraftingRecipeTable.h"
#include "World/Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/ItemDefinition.h"

#include <algorithm>
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
    Tag data = Tag::ofCompound();
    for (int slot = 0; slot < SIZE; ++slot) {
        const ItemStack &item = mInventory.getContainerItem(slot);
        if (!item.isAir() && item.mCount > 0)
            data.put(slotKey("Item", slot), ItemStackNbt::write(item));
        data.putInt(slotKey("ItemTime", slot), mCookTimes[slot]);
    }
    return data;
}

void CampfireBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    bool legacy = data.contains("Items", Tag::Type::List);
    for (int slot = 0; slot < SIZE; ++slot) {
        if (data.contains(slotKey("Item", slot)))
            legacy = false;
    }

    // older Falcon saves used a container list and remaining ticks
    if (legacy) {
        ContainerBlockActor::loadNbt(data, context);
        for (int slot = 0; slot < SIZE; ++slot) {
            const int remaining = std::clamp(data.getInt(slotKey("ItemTime", slot)), 0, COOK_TIME);
            mCookTimes[slot] = mInventory.getContainerItem(slot).isAir() ? 0 : COOK_TIME - remaining;
        }
        return;
    }

    mInventory.clear();
    for (int slot = 0; slot < SIZE; ++slot) {
        const Tag *item = data.get(slotKey("Item", slot));
        if (item != nullptr && item->isCompound())
            mInventory.setContainerItem(slot, ItemStackNbt::read(*item, context));
        mCookTimes[slot] = data.getInt(slotKey("ItemTime", slot));
    }
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
        mCookTimes[slot] = 0;
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

    const ContainerBlockDefinition *definition = ContainerBlock::findDefinition(state->mName);
    if (definition == nullptr || definition->mKind != ContainerBlockKind::Campfire)
        return false;

    const bool lit = state->mStates.getByte(EXTINGUISHED_STATE) == 0;
    bool changed = false;
    for (int slot = 0; slot < SIZE; ++slot) {
        const ItemStack item = mInventory.getContainerItem(slot);
        const std::string identifier = identifierOf(item);
        if (identifier.empty()) {
            mCookTimes[slot] = 0;
            continue;
        }

        if (!lit) {
            mCookTimes[slot] = 0;
            continue;
        }

        const FurnaceRecipeData *recipe = CraftingRecipeTable::findCampfireRecipe(identifier, item.mDamage, _isSoul());
        if (recipe == nullptr) {
            mInventory.setContainerItem(slot, ItemStack::air());
            mCookTimes[slot] = 0;
            ItemActorHandler::dropItem(owner, *mLevel, dropPositionOf(mPosition), item,
                                       ItemActorHandler::randomDropMotion(), ItemActorHandler::DROP_PICKUP_DELAY);
            changed = true;
            continue;
        }

        if (mCookTimes[slot] < COOK_TIME)
            ++mCookTimes[slot];
        if (mCookTimes[slot] < COOK_TIME)
            continue;

        std::shared_ptr<ItemDefinition> output = owner.getItemDefinitions().getDefinition(recipe->mOutputItemId);
        ItemStack cooked = ItemStack::air();
        if (output != nullptr) {
            cooked.mDefinition = std::move(output);
            cooked.mBlockDefinition = owner.getBlockDefinitions().getDefinition(recipe->mOutputItemId);
            cooked.mDamage = 0;
            cooked.mCount = item.mCount;
        }

        if (PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_CAMPFIRE_COOK)) {
            ItemStack input = item;
            PluginEvent event;
            event.mType = FALCON_EVENT_CAMPFIRE_COOK;
            event.mCancellable = true;
            event.mLevel = mLevel;
            event.mBlockPosition = mPosition;
            event.mSourceSlot = slot;
            event.mItem = &input;
            event.mResult = &cooked;
            plugins->dispatch(event);
            if (event.mCancelled)
                continue;
        }
        mInventory.setContainerItem(slot, ItemStack::air());
        mCookTimes[slot] = 0;
        changed = true;
        if (cooked.isAir())
            continue;

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
