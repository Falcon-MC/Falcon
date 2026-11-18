#include "World/Actor/AI/Goal/WorkComposterGoal.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/ComposterBlock.h"
#include "World/Level/Level.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"

#include <algorithm>
#include <utility>

namespace {
    const char *const BONE_MEAL = "minecraft:bone_meal";
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

void WorkComposterGoal::useWorkstation(ServerNetworkHandler &owner, MobActor &mob) {
    if (mNextUseTick == 0)
        mNextUseTick = _nextUseTick(owner);
    if (mInteractions >= mComposting.mBlockInteractionMax || owner.getCurrentTick() < mNextUseTick)
        return;

    // TODO: fill the composter from the inventory once items carry their compost chance
    Level &level = owner.getLevelFor(mob);
    const bool emptied = mComposting.mCanEmpty && ComposterBlock::takeOutput(owner, level, getTargetBlock());
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
