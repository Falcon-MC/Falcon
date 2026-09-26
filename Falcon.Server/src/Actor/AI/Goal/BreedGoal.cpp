#include "Actor/AI/Goal/BreedGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cstring>

namespace {
    const float SEARCH_RANGE_SQUARED = 64.0f;
    const float BREED_DISTANCE_SQUARED = 9.0f;
    const int32_t BREED_TICKS = 60;
    const int32_t REPATH_INTERVAL = 10;
    const float BABY_SCALE = 0.5f;
    const int32_t MIN_EXPERIENCE = 1;
    const int32_t MAX_EXPERIENCE = 7;

    const json::Value *mateEntry(const MobActor &mob, const std::string &mate) {
        const json::Value *breedable = mob.getComponent("minecraft:breedable");
        const json::Value *breedsWith = breedable == nullptr ? nullptr : breedable->get("breeds_with");
        if (breedsWith == nullptr)
            return nullptr;

        if (breedsWith->isArray()) {
            for (const std::unique_ptr<json::Value> &entry: breedsWith->mArray) {
                const json::Value *type = entry->get("mate_type");
                if (type != nullptr && type->string() == mate)
                    return entry.get();
            }
            return nullptr;
        }

        const json::Value *type = breedsWith->get("mate_type");
        if (type != nullptr)
            return type->string() == mate ? breedsWith : nullptr;

        return breedsWith->get(mate);
    }

    bool canMateWith(const MobActor &mob, const MobActor &other) {
        const json::Value *breedable = mob.getComponent("minecraft:breedable");
        if (breedable == nullptr || breedable->get("breeds_with") == nullptr)
            return std::strcmp(mob.getIdentifier(), other.getIdentifier()) == 0;

        return mateEntry(mob, other.getIdentifier()) != nullptr;
    }
}

BreedGoal::BreedGoal(float speed) : mSpeed(speed) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

MobActor *BreedGoal::_partner(ServerNetworkHandler &owner) const {
    return dynamic_cast<MobActor *>(owner.getActor(mPartnerId));
}

MobActor *BreedGoal::_findPartner(ServerNetworkHandler &owner, const MobActor &mob) const {
    MobActor *nearest = nullptr;
    float nearestDistance = SEARCH_RANGE_SQUARED;

    for (auto &entry: owner.getActors()) {
        MobActor *candidate = dynamic_cast<MobActor *>(entry.second.get());
        if (candidate == nullptr || candidate == &mob || !candidate->isAlive() || !candidate->isInLove()
            || candidate->getDimension() != mob.getDimension() || !canMateWith(mob, *candidate))
            continue;

        const float distance = mob.distanceSquaredTo(*candidate);
        if (distance < nearestDistance) {
            nearest = candidate;
            nearestDistance = distance;
        }
    }
    return nearest;
}

bool BreedGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (!mob.isInLove())
        return false;

    const MobActor *partner = _findPartner(owner, mob);
    if (partner == nullptr)
        return false;

    mPartnerId = partner->getUniqueId();
    return true;
}

bool BreedGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    const MobActor *partner = _partner(owner);
    return mob.isInLove() && partner != nullptr && partner->isAlive() && partner->isInLove()
           && mLoveTime < BREED_TICKS;
}

void BreedGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    mLoveTime = 0;
}

void BreedGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mPartnerId = 0;
    mLoveTime = 0;
    mob.getNavigation().stop(mob);
}

void BreedGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    MobActor *partner = _partner(owner);
    if (partner == nullptr)
        return;

    mob.getLookControl().setLookAt(partner->getPosition());
    if (mLoveTime % REPATH_INTERVAL == 0)
        mob.getNavigation().moveTo(partner->getPosition(), mSpeed);

    ++mLoveTime;
    if (mLoveTime >= BREED_TICKS && mob.distanceSquaredTo(*partner) < BREED_DISTANCE_SQUARED)
        _breed(owner, mob, *partner);
}

void BreedGoal::_breed(ServerNetworkHandler &owner, MobActor &mob, MobActor &partner) {
    std::string babyType = mob.getIdentifier();
    const json::Value *entry = mateEntry(mob, partner.getIdentifier());
    const json::Value *baby = entry == nullptr ? nullptr : entry->get("baby_type");
    if (baby != nullptr && baby->isString())
        babyType = baby->mString;

    mob.finishBreeding(owner);
    partner.finishBreeding(owner);

    Level &level = owner.getLevelFor(mob);
    ServerActor *child = owner.spawnBabyActor(level, babyType, mob.getPosition(), BABY_SCALE);
    if (MobActor *childMob = dynamic_cast<MobActor *>(child)) {
        childMob->setParent(mob.getRuntimeId());
        childMob->inheritVariant(mob, partner);
    }
    const int experience = mob.getBreedingExperience();
    owner.spawnExperienceOrbs(level, mob.getPosition(),
                              experience >= 0 ? experience : MobActor::randomRange(MIN_EXPERIENCE, MAX_EXPERIENCE));
}
