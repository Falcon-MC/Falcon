#include "World/Actor/AI/Goal/DrinkPotionGoal.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Item/PotionItem.h"
#include "World/Item/VanillaItems.h"

#include <random>

namespace {
    const char *const POTION = "minecraft:potion";
    // the game asks again on every sixth call
    const int CALLS_BETWEEN_TRIES = 5;

    std::mt19937 &drinkRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    bool holdsPotion(const MobActor &mob) {
        const ItemStack &held = mob.getEquipment().getSlot(MobEquipment::MAINHAND);
        return !held.isAir()
               && dynamic_cast<const PotionItem *>(VanillaItems::fromIdentifier(held.mDefinition->getIdentifier()))
                          != nullptr;
    }

    void setDrinking(ServerNetworkHandler &owner, MobActor &mob, bool drinking) {
        if (mob.getFlags().get(ActorFlag::UsingItem) == drinking)
            return;
        mob.getFlags().set(ActorFlag::UsingItem, drinking);
        owner.syncActorFlags(mob);
    }
}

DrinkPotionGoal::DrinkPotionGoal(std::vector<Potion> potions, float speedModifier)
        : mPotions(std::move(potions)), mSpeedModifier(speedModifier) {}

bool DrinkPotionGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mCalls++ < CALLS_BETWEEN_TRIES)
        return false;
    mCalls = 0;

    // a draw for each potion in turn, then its filters with the target as the other
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const Actor *target = mob.getTarget(owner);
    const Potion *chosen = nullptr;
    for (const Potion &potion: mPotions) {
        if (unit(drinkRandom()) >= potion.mChance)
            continue;
        if (potion.mFilters == nullptr || EntityFilter::test(*potion.mFilters, owner, mob, target)) {
            chosen = &potion;
            break;
        }
    }
    if (chosen == nullptr)
        return false;

    ItemStack potion = owner.createItemStack(POTION, 1);
    if (potion.isAir())
        return false;
    const int64_t duration = ServerNetworkHandler::useDurationTicksFor(potion);
    potion.mDamage = chosen->mId;
    mob.getEquipment().setSlot(MobEquipment::MAINHAND, std::move(potion));
    mob.getEquipment().broadcast(owner, mob);
    mEndTick = owner.getCurrentTick() + duration;
    setDrinking(owner, mob, true);
    mob.setMovementModifier(mSpeedModifier);
    return true;
}

bool DrinkPotionGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    return mob.getFlags().get(ActorFlag::UsingItem);
}

void DrinkPotionGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (owner.getCurrentTick() < mEndTick)
        return;
    setDrinking(owner, mob, false);
    if (holdsPotion(mob))
        owner.applyPotionEffects(mob, mob.getEquipment().getSlot(MobEquipment::MAINHAND).mDamage, 1.0f);
}

void DrinkPotionGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    mob.getEquipment().setSlot(MobEquipment::MAINHAND, ItemStack::air());
    mob.getEquipment().broadcast(owner, mob);
    setDrinking(owner, mob, false);
    mob.setMovementModifier(0.0f);
    mEndTick = 0;
}
