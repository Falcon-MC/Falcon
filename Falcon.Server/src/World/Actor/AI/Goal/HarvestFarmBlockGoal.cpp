#include "World/Actor/AI/Goal/HarvestFarmBlockGoal.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Item/FarmSeedItem.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Block/CropBlock.h"
#include "World/Level/Block/FarmlandBlock.h"
#include "World/Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"

#include <string>

namespace {
    const char *const GRIEFING_RULE = "mobgriefing";
    const char *const GROWTH_STATE = "growth";
    // the game takes a crop as ripe at growth 7 alone
    const int32_t RIPE_GROWTH = 7;

    bool isRipeCrop(const BlockState &state) {
        return CropBlock::matches(state.mName) && state.mStates.getInt(GROWTH_STATE, 0) == RIPE_GROWTH;
    }

    const BlockState *above(Level &level, const Vector3i &position) {
        return level.peekBlockPtr(position.x, position.y + 1, position.z);
    }
}

HarvestFarmBlockGoal::HarvestFarmBlockGoal(Settings settings, Timing timing)
        : BaseMoveToBlockGoal(settings), mTiming(timing) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look
                            | (uint8_t) GoalControlFlag::Jump);
}

int HarvestFarmBlockGoal::findSeedSlot(const MobActor &mob) {
    const MobEquipment &equipment = mob.getEquipment();
    for (int slot = 0; slot < equipment.getInventorySize(); ++slot) {
        if (FarmSeedItem::getSeedComponent(equipment.getInventoryItem(slot)) != nullptr)
            return slot;
    }
    return -1;
}

bool HarvestFarmBlockGoal::plantSeed(ServerNetworkHandler &owner, MobActor &mob, int slot, const Vector3i &farmland) {
    MobEquipment &equipment = mob.getEquipment();
    ItemStack item = equipment.getInventoryItem(slot);
    const Tag *seed = FarmSeedItem::getSeedComponent(item);
    Level &level = owner.getLevelFor(mob);
    const BlockState *ground = level.peekBlockPtr(farmland.x, farmland.y, farmland.z);
    const BlockState *space = above(level, farmland);
    if (seed == nullptr || ground == nullptr || space == nullptr || space->mName != BlockNames::AIR
        || !FarmSeedItem::plantsOn(*seed, ground->mName))
        return false;

    const std::string crop = seed->getString("crop_result");
    const Tag *states = BlockPaletteRegistry::getInstance().getDefaultStates(crop);
    if (states == nullptr)
        return false;

    level.setBlock(Vector3i(farmland.x, farmland.y + 1, farmland.z), BlockState(crop, *states), true);
    item.mCount -= 1;
    equipment.setInventoryItem(slot, item.mCount > 0 ? item : ItemStack::air());
    return true;
}

bool HarvestFarmBlockGoal::isValidTarget(ServerNetworkHandler &owner, MobActor &mob, const Vector3i &position) {
    Level &level = owner.getLevelFor(mob);
    const BlockState *ground = level.peekBlockPtr(position.x, position.y, position.z);
    if (ground == nullptr || !FarmlandBlock::matches(ground->mName))
        return false;

    const BlockState *space = above(level, position);
    if (space == nullptr)
        return false;
    if (mSeedSlot >= 0 && space->mName == BlockNames::AIR)
        return true;
    return isRipeCrop(*space);
}

bool HarvestFarmBlockGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mCooldownTicks > 0) {
        --mCooldownTicks;
        return false;
    }

    mCooldownTicks = MobActor::randomRange(0, mTiming.mMaxTicksBeforeSearch - 1);
    if (mob.getInventoryCapacity() <= 0)
        return false;

    Level &level = owner.getLevelFor(mob);
    if (!level.getGameRules().getBool(GRIEFING_RULE))
        return false;

    mTask = Task::None;
    mSeedSlot = findSeedSlot(mob);
    if (findTarget(owner, mob)) {
        const BlockState *space = above(level, getTarget());
        if (space != nullptr) {
            if (mSeedSlot >= 0 && space->mName == BlockNames::AIR) {
                mTask = Task::Plant;
                return true;
            }
            if (isRipeCrop(*space)) {
                mTask = Task::Harvest;
                return true;
            }
        }
    }

    mCooldownTicks = MobActor::randomRange(0, mTiming.mSearchCooldownTicks - 1);
    return false;
}

bool HarvestFarmBlockGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    return mTask != Task::None && BaseMoveToBlockGoal::canContinueToUse(owner, mob);
}

void HarvestFarmBlockGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    BaseMoveToBlockGoal::tick(owner, mob);
    const Vector3i &target = getTarget();
    mob.getLookControl().setLookAt(
            Vector3f((float) target.x + 0.5f, (float) target.y + 1.0f, (float) target.z + 0.5f));
    if (!isReached())
        return;

    Level &level = owner.getLevelFor(mob);
    if (mTask == Task::Harvest) {
        const Vector3i crop(target.x, target.y + 1, target.z);
        const BlockState *state = above(level, target);
        if (state == nullptr || !isRipeCrop(*state)) {
            mTask = Task::None;
            mCooldownTicks = mTiming.mTicksUntilNewTask;
            return;
        }
        const BlockState broken = *state;
        BlockActionHandler::destroyBlock(owner, level, crop, broken, true, ItemStack::air(), &mob);
    }

    // a harvest replants at once when a seed is at hand
    if (mSeedSlot >= 0)
        plantSeed(owner, mob, mSeedSlot, target);
    mTask = Task::None;
    mCooldownTicks = mTiming.mTicksUntilNewTask;
}
