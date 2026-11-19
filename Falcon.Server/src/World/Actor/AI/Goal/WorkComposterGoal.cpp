#include "World/Actor/AI/Goal/WorkComposterGoal.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Item/VanillaItems.h"
#include "World/Level/Block/ComposterBlock.h"
#include "World/Level/Level.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"

#include <algorithm>
#include <string>
#include <utility>

namespace {
    const char *const BONE_MEAL = "minecraft:bone_meal";

    bool isFarmerCompost(const ItemStack &item) {
        if (item.isAir())
            return false;
        const Item *type = VanillaItems::fromIdentifier(item.mDefinition->getIdentifier());
        return type != nullptr && type->isFarmerCompost();
    }

    int32_t countOf(const MobEquipment &equipment, const std::string &identifier) {
        int32_t count = 0;
        for (int slot = 0; slot < equipment.getInventorySize(); ++slot) {
            const ItemStack &item = equipment.getInventoryItem(slot);
            if (!item.isAir() && item.mDefinition->getIdentifier() == identifier)
                count += item.mCount;
        }
        return count;
    }
}

WorkComposterGoal::WorkComposterGoal(Settings settings, Composting composting)
        : WorkGoal(std::move(settings)), mComposting(composting) {}

void WorkComposterGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    WorkGoal::start(owner, mob);
    mInteractions = 0;
    mNextUseTick = 0;
}

int64_t WorkComposterGoal::_nextUseTick(ServerNetworkHandler &owner) const {
    return owner.getCurrentTick()
           + MobActor::randomRange(mComposting.mUseBlockMin,
                                   std::max(mComposting.mUseBlockMin, mComposting.mUseBlockMax - 1));
}

// the last slot holding more of a seed than the farmer keeps; the surplus goes in, as many as the use allows
bool WorkComposterGoal::_fill(ServerNetworkHandler &owner, MobActor &mob) {
    if (mComposting.mItemsPerUseMax == 0)
        return false;

    MobEquipment &equipment = mob.getEquipment();
    for (int slot = equipment.getInventorySize() - 1; slot >= 0; --slot) {
        ItemStack item = equipment.getInventoryItem(slot);
        if (!isFarmerCompost(item))
            continue;
        const int32_t surplus = countOf(equipment, item.mDefinition->getIdentifier()) - mComposting.mMinItemCount;
        if (surplus <= 0)
            continue;

        int32_t amount = std::min(item.mCount, surplus);
        if (mComposting.mItemsPerUseMax > 0)
            amount = std::min(amount, mComposting.mItemsPerUseMax);
        const int32_t used = ComposterBlock::addItems(owner, owner.getLevelFor(mob), getTargetBlock(), item, amount);
        if (used <= 0)
            return false;
        item.mCount -= used;
        equipment.setInventoryItem(slot, item.mCount <= 0 ? ItemStack::air() : std::move(item));
        return true;
    }
    return false;
}

void WorkComposterGoal::useWorkstation(ServerNetworkHandler &owner, MobActor &mob) {
    if (mNextUseTick == 0)
        mNextUseTick = _nextUseTick(owner);
    if (mInteractions >= mComposting.mBlockInteractionMax || owner.getCurrentTick() < mNextUseTick)
        return;

    Level &level = owner.getLevelFor(mob);
    const bool emptied = mComposting.mCanEmpty && ComposterBlock::takeOutput(owner, level, getTargetBlock());
    if (!emptied && mComposting.mCanFill && _fill(owner, mob)) {
        ++mInteractions;
        mNextUseTick = _nextUseTick(owner);
        return;
    }
    if (emptied) {
        const ItemStack boneMeal = owner.createItemStack(BONE_MEAL, 1);
        if (!mob.getEquipment().addInventoryItem(boneMeal, mob.getInventoryCapacity()))
            owner.dropItem(level, mob.getPosition(), boneMeal, ItemActorHandler::randomDropMotion(),
                           ItemActorHandler::DROP_PICKUP_DELAY);
    } else {
        playWorkSound(owner, mob);
    }

    ++mInteractions;
    mNextUseTick = _nextUseTick(owner);
}
