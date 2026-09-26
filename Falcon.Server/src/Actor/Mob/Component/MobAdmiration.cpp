#include "Actor/Mob/Component/MobAdmiration.h"

#include "Actor/Mob/Component/MobShareables.h"
#include "Actor/Mob/MobActor.h"
#include "Level/Level.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>

namespace {
    const char *const ADMIRE_ITEM_COMPONENT = "minecraft:admire_item";
    const char *const BARTER_COMPONENT = "minecraft:barter";
    const int32_t TICKS_PER_SECOND = 20;
    const float DEFAULT_ADMIRE_SECONDS = 10.0f;
    const float BARTER_DROP_HEIGHT = 1.3f;

    float numberIn(const json::Value *component, const char *key, float fallback) {
        const json::Value *value = component == nullptr ? nullptr : component->get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }

    void setAdmiringFlag(ServerNetworkHandler &owner, MobActor &mob, bool admiring) {
        if (mob.getFlags().get(ActorFlag::Admiring) == admiring)
            return;

        mob.getFlags().set(ActorFlag::Admiring, admiring);
        owner.syncActorFlags(mob);
    }

    ItemStack takeOffhand(ServerNetworkHandler &owner, MobActor &mob) {
        MobEquipment &equipment = mob.getEquipment();
        ItemStack item = equipment.getSlot(MobEquipment::OFFHAND);
        equipment.setSlot(MobEquipment::OFFHAND, ItemStack::air());
        equipment.broadcast(owner, mob);
        return item;
    }
}

bool MobAdmiration::canAdmire(ServerNetworkHandler &owner, const MobActor &mob) const {
    const json::Value *component = mob.getComponent(ADMIRE_ITEM_COMPONENT);
    if (component == nullptr || isAdmiring() || !mob.getEquipment().getSlot(MobEquipment::OFFHAND).isAir())
        return false;

    const int64_t cooldown = (int64_t) std::lround(numberIn(component, "cooldown_after_being_attacked", 0.0f)
                                                   * (float) TICKS_PER_SECOND);
    return owner.getCurrentTick() - mob.getLastHurtTick() >= cooldown;
}

void MobAdmiration::start(ServerNetworkHandler &owner, MobActor &mob, const ItemStack &item, bool barter) {
    ItemStack admired = item;
    admired.mCount = 1;

    MobEquipment &equipment = mob.getEquipment();
    equipment.setSlot(MobEquipment::OFFHAND, std::move(admired));
    equipment.broadcast(owner, mob);

    const float seconds = numberIn(mob.getComponent(ADMIRE_ITEM_COMPONENT), "duration", DEFAULT_ADMIRE_SECONDS);
    mTicks = std::max(1, (int32_t) std::lround(seconds * (float) TICKS_PER_SECOND));
    mBarter = barter && mob.getComponent(BARTER_COMPONENT) != nullptr;
    mob.getNavigation().stop(mob);
    setAdmiringFlag(owner, mob, true);
}

void MobAdmiration::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (mTicks <= 0)
        return;

    if (mob.getEquipment().getSlot(MobEquipment::OFFHAND).isAir()) {
        mTicks = 0;
        setAdmiringFlag(owner, mob, false);
        return;
    }

    if (--mTicks == 0)
        _finish(owner, mob);
}

void MobAdmiration::abort(ServerNetworkHandler &owner, MobActor &mob) {
    if (mTicks <= 0)
        return;

    mTicks = 0;
    setAdmiringFlag(owner, mob, false);

    const ItemStack item = takeOffhand(owner, mob);
    if (!item.isAir())
        owner.dropItem(owner.getLevelFor(mob), mob.getPosition(), item, ItemActorHandler::randomDropMotion(),
                       ItemActorHandler::DROP_PICKUP_DELAY);
}

void MobAdmiration::_finish(ServerNetworkHandler &owner, MobActor &mob) {
    setAdmiringFlag(owner, mob, false);
    const ItemStack item = takeOffhand(owner, mob);
    if (item.isAir())
        return;

    const json::Value *barter = mob.getComponent(BARTER_COMPONENT);
    const json::Value *table = barter == nullptr ? nullptr : barter->get("barter_table");
    if (!mBarter || table == nullptr || mob.getComponent("minecraft:is_baby") != nullptr) {
        MobShareables::store(owner, mob, item);
        return;
    }

    Level &level = owner.getLevelFor(mob);
    const Vector3f position = mob.getPosition();
    const Vector3f dropPosition(position.x, position.y + BARTER_DROP_HEIGHT, position.z);
    for (const ItemStack &loot: mob.rollLoot(owner, table->string()))
        owner.dropItem(level, dropPosition, loot, ItemActorHandler::randomDropMotion(),
                       ItemActorHandler::DROP_PICKUP_DELAY);
}
