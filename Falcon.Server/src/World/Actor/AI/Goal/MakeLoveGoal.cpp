#include "World/Actor/AI/Goal/MakeLoveGoal.h"

#include "World/Actor/AI/Navigation/Path.h"
#include "World/Actor/AI/Navigation/PathFinder.h"
#include "World/Actor/Mob/Passive/VillagerV2Actor.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Level/Level.h"
#include "World/Village/Village.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/ActorEventPacket.h"

#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>

namespace {
    const int32_t START_CHANCE = 500;
    const int32_t LOVE_TICKS = 300;
    const float BREED_DISTANCE_SQUARED = 2.25f;
    const float WALK_SPEED_MULTIPLIER = 0.25f;
    // the partner search box: the villager's box grown by 8 across, 3 up and down
    const float SEARCH_HORIZONTAL = 8.0f;
    const float SEARCH_VERTICAL = 3.0f;
    const int32_t HEARTS_CHANCE = 35;
    // the baby group's minecraft:scale
    const float BABY_SCALE = 0.5f;

    bool hasBreedingFood(const VillagerV2Actor &villager) {
        return villager.isWilling() || villager.findBreedingFoodSlot() >= 0;
    }

    bool isBaby(const MobActor &mob) {
        return mob.getFlags().get(ActorFlag::Baby);
    }

    // a dweller without a village never breeds; a mob that dwells nowhere is not asked
    bool villageHasRoom(const MobActor &mob) {
        const MobDweller &dweller = mob.getDweller();
        if (!dweller.isActive())
            return true;
        const std::shared_ptr<Village> village = dweller.getVillage();
        return village != nullptr && village->hasRoomForBaby();
    }

    bool endsAt(const Path &path, const Actor &target) {
        if (path.isEmpty())
            return false;
        const Vector3f &last = path.last();
        const Vector3f &position = target.getPosition();
        return (int32_t) std::floor(last.x) == (int32_t) std::floor(position.x)
               && (int32_t) std::floor(last.z) == (int32_t) std::floor(position.z);
    }

    // the game asks for a path from the villager to the partner's column
    bool canWalkTo(ServerNetworkHandler &owner, MobActor &mob, const Actor &target) {
        const Path *current = mob.getNavigation().getPath();
        if (current != nullptr && endsAt(*current, target))
            return true;

        // the server's path search returns nothing for the block next to the start: one step is always a path
        const Vector3f &from = mob.getPosition();
        const Vector3f &to = target.getPosition();
        if (std::abs((int32_t) std::floor(from.x) - (int32_t) std::floor(to.x)) <= 1
            && std::abs((int32_t) std::floor(from.z) - (int32_t) std::floor(to.z)) <= 1
            && (int32_t) std::floor(from.y) == (int32_t) std::floor(to.y))
            return true;

        Path path;
        return PathFinder::get().findPath(owner.getLevelFor(mob), mob, target.getPosition(),
                                          mob.getNavigation().getOptions(), path)
               && endsAt(path, target);
    }
}

MakeLoveGoal::MakeLoveGoal() {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

VillagerV2Actor *MakeLoveGoal::findPartner(ServerNetworkHandler &owner, const VillagerV2Actor &villager) {
    if (!villager.hasLovePartner())
        return nullptr;
    return dynamic_cast<VillagerV2Actor *>(owner.getActor(villager.getLovePartnerId()));
}

bool MakeLoveGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    VillagerV2Actor &villager = static_cast<VillagerV2Actor &>(mob);
    if (villager.getFlags().get(ActorFlag::InLove) && !villager.isLoveInitiator())
        return false;
    if (isBaby(villager) || MobActor::randomRange(0, START_CHANCE - 1) != 0)
        return false;
    if (!villageHasRoom(villager))
        return false;

    VillagerV2Actor *partner = _choosePartner(owner, villager);
    if (partner == nullptr)
        return false;

    villager.setLovePartner(owner, partner);
    villager.setLoveInitiator(true);
    partner->setLovePartner(owner, &villager);
    partner->setLoveInitiator(false);
    // the partner drops what it was doing for its receive_love goal
    partner->stopGoals(owner);
    return true;
}

bool MakeLoveGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    VillagerV2Actor &villager = static_cast<VillagerV2Actor &>(mob);
    if (mLoveTicks < 0 || isBaby(villager) || !villager.getFlags().get(ActorFlag::InLove))
        return false;
    if (!villageHasRoom(villager))
        return false;

    const VillagerV2Actor *partner = findPartner(owner, villager);
    if (partner == nullptr || !hasBreedingFood(*partner))
        return false;
    if (villager.distanceSquaredTo(*partner) <= BREED_DISTANCE_SQUARED)
        return true;
    return canWalkTo(owner, villager, *partner);
}

void MakeLoveGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    mLoveTicks = LOVE_TICKS;
}

void MakeLoveGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    VillagerV2Actor &villager = static_cast<VillagerV2Actor &>(mob);
    if (VillagerV2Actor *partner = findPartner(owner, villager))
        partner->setLovePartner(owner, nullptr);
    villager.setLovePartner(owner, nullptr);
}

void MakeLoveGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    VillagerV2Actor &villager = static_cast<VillagerV2Actor &>(mob);
    --mLoveTicks;
    VillagerV2Actor *partner = findPartner(owner, villager);
    if (partner == nullptr)
        return;

    villager.getLookControl().setLookAt(partner->getPosition());
    // the partner only walks up from 3 blocks away: the initiator closes the rest
    if (villager.distanceSquaredTo(*partner) > BREED_DISTANCE_SQUARED)
        villager.getNavigation().moveTo(partner->getPosition(), villager.getMovementSpeed() * WALK_SPEED_MULTIPLIER);
    if (mLoveTicks == 0 && partner->getFlags().get(ActorFlag::InLove)
        && villager.distanceSquaredTo(*partner) <= BREED_DISTANCE_SQUARED)
        _spawnBaby(owner, villager, *partner);

    if (MobActor::randomRange(0, HEARTS_CHANCE - 1) == 0)
        owner.broadcastActorEvent(villager, EntityEventType::LoveHearts);
}

VillagerV2Actor *MakeLoveGoal::_choosePartner(ServerNetworkHandler &owner, VillagerV2Actor &villager) const {
    const AxisAlignedBB area = ActorPushSystem::boundingBoxOf(villager).expand(SEARCH_HORIZONTAL, SEARCH_VERTICAL,
                                                                                  SEARCH_HORIZONTAL);
    const float reach = std::sqrt(2.0f * SEARCH_HORIZONTAL * SEARCH_HORIZONTAL + SEARCH_VERTICAL * SEARCH_VERTICAL)
                        + villager.getSize().mHeight;

    VillagerV2Actor *chosen = nullptr;
    float closest = FLT_MAX;
    owner.forEachActorNear(villager.getPosition(), reach, [&](ServerActor &actor) {
        VillagerV2Actor *candidate = dynamic_cast<VillagerV2Actor *>(&actor);
        if (chosen != nullptr && chosen->getLovePartnerId() == villager.getUniqueId())
            return;
        if (candidate == nullptr || candidate == &villager || !candidate->isAlive()
            || !candidate->sharesLevelWith(villager)
            || !ActorPushSystem::boundingBoxOf(*candidate).intersectsWith(area))
            return;

        // one already paired with this villager is taken at once
        if (candidate->hasLovePartner()) {
            if (candidate->getLovePartnerId() == villager.getUniqueId())
                chosen = candidate;
            return;
        }

        const float distance = villager.distanceSquaredTo(*candidate);
        if (distance > closest || isBaby(*candidate) || !hasBreedingFood(*candidate)
            || !canWalkTo(owner, villager, *candidate))
            return;
        closest = distance;
        chosen = candidate;
    });
    return chosen;
}

void MakeLoveGoal::_spawnBaby(ServerNetworkHandler &owner, VillagerV2Actor &villager, VillagerV2Actor &partner) {
    EventSignal<ActorBreedBeforeEvent> &breed = owner.getEventBus().before().mActorBreed;
    if (breed.hasSubscribers()) {
        ActorBreedBeforeEvent event(villager, partner, villager.getIdentifier());
        breed.emit(event);
        if (event.isCancelled())
            return;
    }

    ServerActor *baby = owner.spawnBabyActor(owner.getLevelFor(villager), villager.getIdentifier(),
                                             villager.getPosition(), BABY_SCALE);
    if (MobActor *babyMob = dynamic_cast<MobActor *>(baby))
        babyMob->setParent(villager.getRuntimeId());

    for (VillagerV2Actor *parent: {&villager, &partner}) {
        if (!parent->isWilling())
            parent->consumeBreedingFood();
        parent->setWilling(false);
    }
}
