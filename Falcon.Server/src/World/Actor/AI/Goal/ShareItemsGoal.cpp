#include "World/Actor/AI/Goal/ShareItemsGoal.h"

#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/Component/MobShareables.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Level/Level.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>
#include <utility>

namespace {
    const char *const SHAREABLES = "minecraft:shareables";
    const int32_t THROW_DELAY_TICKS = 20;
    const float KEEP_DISTANCE_SQUARED = 16.0f;
    const float THROW_SPEED = 0.1f;
    const float THROW_LIFT = 0.2f;
    // the game throws from the mob's drop point, which the server has no model for: its eyes stand in
    const float EYE_HEIGHT_RATIO = 0.85f;

    struct Surplus {
        int mSlot;
        ItemStack mItem;
    };

    std::vector<Surplus> surplusOf(const MobActor &mob) {
        std::vector<Surplus> surplus;
        const MobEquipment &equipment = mob.getEquipment();
        for (int slot = 0; slot < equipment.getInventorySize(); ++slot) {
            const ItemStack &item = equipment.getInventoryItem(slot);
            if (item.isAir())
                continue;

            bool listed = false;
            for (const Surplus &entry: surplus)
                listed = listed || entry.mItem.mDefinition == item.mDefinition;
            if (!listed && MobShareables::surplusOf(mob, item) > 0)
                surplus.push_back({slot, item});
        }
        return surplus;
    }

    void removeFromInventory(MobActor &mob, const ItemStack &item, int32_t count) {
        MobEquipment &equipment = mob.getEquipment();
        for (int slot = 0; slot < equipment.getInventorySize() && count > 0; ++slot) {
            ItemStack held = equipment.getInventoryItem(slot);
            if (held.isAir() || held.mDefinition != item.mDefinition)
                continue;

            const int32_t taken = std::min(count, held.mCount);
            held.mCount -= taken;
            count -= taken;
            equipment.setInventoryItem(slot, held.mCount > 0 ? held : ItemStack::air());
        }
    }
}

ShareItemsGoal::ShareItemsGoal(Settings settings) : mSettings(std::move(settings)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

bool ShareItemsGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mob.getComponent(SHAREABLES) == nullptr || mob.getInventoryCapacity() <= 0)
        return false;

    const std::vector<Surplus> surplus = surplusOf(mob);
    if (surplus.empty())
        return false;

    const Vector3f &position = mob.getPosition();
    const float reach = (float) mSettings.mMaxDistance;
    const AxisAlignedBB area(position.x - reach, position.y - reach, position.z - reach, position.x + reach,
                             position.y + reach, position.z + reach);
    std::vector<MobActor *> nearby;
    owner.forEachActorNear(position, reach * std::sqrt(3.0f) + mob.getSize().mHeight, [&](ServerActor &actor) {
        MobActor *candidate = dynamic_cast<MobActor *>(&actor);
        if (candidate != nullptr && candidate != &mob && candidate->isAlive() && candidate->sharesLevelWith(mob)
            && candidate->getComponent(SHAREABLES) != nullptr
            && ActorPushSystem::boundingBoxOf(*candidate).intersectsWith(area))
            nearby.push_back(candidate);
    });

    // the entity types in their order, then the mobs, then the items
    for (const std::shared_ptr<json::Value> &filters: mSettings.mFilters) {
        for (MobActor *candidate: nearby) {
            if (filters != nullptr && !EntityFilter::test(*filters, owner, mob, candidate))
                continue;

            for (const Surplus &entry: surplus) {
                if (MobShareables::wantedCountOf(*candidate, entry.mItem) <= 0)
                    continue;

                int32_t consumed = 0;
                ItemStack shared = MobShareables::sharedStack(owner, mob, entry.mItem,
                                                              MobShareables::surplusOf(mob, entry.mItem), consumed);
                if (shared.isAir() || shared.mCount <= 0 || !mob.setTarget(owner, candidate->getRuntimeId()))
                    return false;

                removeFromInventory(mob, entry.mItem, consumed);
                mItem = std::move(shared);
                return true;
            }
        }
    }
    return false;
}

bool ShareItemsGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = mob.getTarget(owner);
    return target != nullptr && target->isAlive() && mob.distanceSquaredTo(*target) <= KEEP_DISTANCE_SQUARED;
}

void ShareItemsGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = mob.getTarget(owner);
    if (target == nullptr)
        return;

    // the item rides in the hand on the way
    mob.getEquipment().setSlot(MobEquipment::MAINHAND, mItem);
    mob.getEquipment().broadcast(owner, mob);
    mThrowTicks = THROW_DELAY_TICKS;
    mob.getNavigation().moveTo(target->getPosition(), mSettings.mSpeed);
}

void ShareItemsGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    if (!mItem.isAir())
        MobShareables::store(owner, mob, mItem);
    mob.getNavigation().stop(mob);
    mItem = ItemStack::air();
    mob.getEquipment().setSlot(MobEquipment::MAINHAND, ItemStack::air());
    mob.getEquipment().broadcast(owner, mob);
    mThrowTicks = 0;
}

void ShareItemsGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = mob.getTarget(owner);
    if (target == nullptr)
        return;

    const Vector3f &targetPosition = target->getPosition();
    mob.getLookControl().setLookAt(targetPosition);

    // the game's closeness test measures the target against itself: the throw only waits out its delay
    const int32_t left = mThrowTicks--;
    if (left <= 0) {
        if (mItem.isAir())
            return;
        mThrowTicks = 0;
    } else if (left != 1) {
        return;
    }

    const Vector3f &position = mob.getPosition();
    const Vector3f from(position.x, position.y + mob.getSize().mHeight * EYE_HEIGHT_RATIO, position.z);
    const Vector3f motion((targetPosition.x - from.x) * THROW_SPEED,
                          (targetPosition.y - from.y) * THROW_SPEED + THROW_LIFT,
                          (targetPosition.z - from.z) * THROW_SPEED);
    owner.dropItem(owner.getLevelFor(mob), from, mItem, motion, ItemActorHandler::DROP_PICKUP_DELAY);
    mItem = ItemStack::air();
    mob.getEquipment().setSlot(MobEquipment::MAINHAND, ItemStack::air());
    mob.getEquipment().broadcast(owner, mob);
}
