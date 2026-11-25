#include "World/Actor/AI/Goal/FertilizeFarmBlockGoal.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Item/Item.h"
#include "World/Item/VanillaItems.h"
#include "World/Level/Block/CropBlock.h"
#include "World/Level/Block/FarmlandBlock.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

namespace {
    const char *const GRIEFING_RULE = "mobgriefing";
    const char *const GROWTH_STATE = "growth";
    // the game takes a crop as ripe at growth 7 alone
    const int32_t RIPE_GROWTH = 7;

    const Item *fertilizerOf(const ItemStack &item) {
        if (item.isAir() || item.mCount <= 0)
            return nullptr;
        const Item *type = VanillaItems::fromIdentifier(item.mDefinition->getIdentifier());
        return type != nullptr && type->isFertilizer() ? type : nullptr;
    }

    int findFertilizerSlot(const MobActor &mob) {
        const MobEquipment &equipment = mob.getEquipment();
        for (int slot = 0; slot < equipment.getInventorySize(); ++slot) {
            if (fertilizerOf(equipment.getInventoryItem(slot)) != nullptr)
                return slot;
        }
        return -1;
    }

    const BlockState *cropAbove(Level &level, const Vector3i &position) {
        const BlockState *state = level.peekBlockPtr(position.x, position.y + 1, position.z);
        return state != nullptr && CropBlock::matches(state->mName) ? state : nullptr;
    }
}

FertilizeFarmBlockGoal::FertilizeFarmBlockGoal(Settings settings, int32_t maxUses, int32_t searchCooldownTicks)
        : BaseMoveToBlockGoal(settings), mMaxUses(maxUses), mSearchCooldownTicks(searchCooldownTicks) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look
                            | (uint8_t) GoalControlFlag::Jump);
}

bool FertilizeFarmBlockGoal::isValidTarget(ServerNetworkHandler &owner, MobActor &mob, const Vector3i &position) {
    Level &level = owner.getLevelFor(mob);
    const BlockState *ground = level.peekBlockPtr(position.x, position.y, position.z);
    if (ground == nullptr || !FarmlandBlock::matches(ground->mName))
        return false;
    const BlockState *crop = cropAbove(level, position);
    return crop != nullptr && crop->mStates.getInt(GROWTH_STATE, 0) != RIPE_GROWTH;
}

bool FertilizeFarmBlockGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    const int64_t now = owner.getCurrentTick();
    if (now < mNextSearchTick)
        return false;

    mNextSearchTick = now + MobActor::randomRange(0, mSearchCooldownTicks - 1);
    if (mob.getInventoryCapacity() <= 0 || !owner.getLevelFor(mob).getGameRules().getBool(GRIEFING_RULE))
        return false;

    mFertilizerSlot = findFertilizerSlot(mob);
    return mFertilizerSlot >= 0 && findTarget(owner, mob);
}

bool FertilizeFarmBlockGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    return mFertilizerSlot >= 0 && mUses < mMaxUses && BaseMoveToBlockGoal::canContinueToUse(owner, mob);
}

void FertilizeFarmBlockGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    BaseMoveToBlockGoal::start(owner, mob);
    mUses = 0;
}

void FertilizeFarmBlockGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    BaseMoveToBlockGoal::stop(owner, mob);
    mNextSearchTick = owner.getCurrentTick() + MobActor::randomRange(0, mSearchCooldownTicks - 1);
}

void FertilizeFarmBlockGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    BaseMoveToBlockGoal::tick(owner, mob);
    const Vector3i &target = getTarget();
    mob.getLookControl().setLookAt(
            Vector3f((float) target.x + 0.5f, (float) target.y + 1.0f, (float) target.z + 0.5f));
    if (!isReached())
        return;

    // a crop that ripened on the way, or a failed use, ends the goal
    Level &level = owner.getLevelFor(mob);
    const BlockState *crop = cropAbove(level, target);
    MobEquipment &equipment = mob.getEquipment();
    ItemStack fertilizer = equipment.getInventoryItem(mFertilizerSlot);
    const Item *type = fertilizerOf(fertilizer);
    if (crop == nullptr || crop->mStates.getInt(GROWTH_STATE, 0) == RIPE_GROWTH || type == nullptr
        || !type->fertilizeBlock(owner, level, Vector3i(target.x, target.y + 1, target.z))) {
        mFertilizerSlot = -1;
        return;
    }

    fertilizer.mCount -= 1;
    equipment.setInventoryItem(mFertilizerSlot, fertilizer.mCount > 0 ? fertilizer : ItemStack::air());
    ++mUses;
    mFertilizerSlot = findFertilizerSlot(mob);
}
