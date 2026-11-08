#include "World/Actor/Mob/Component/MobDweller.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Level.h"
#include "World/Village/VillageManager.h"
#include "Network/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace {
    const char *const DWELLER_COMPONENT = "minecraft:dweller";
    const float DEFAULT_BOUNDS_TOLERANCE = 10.0f; // 0x14efd22ac
    const int32_t MIGRATION_RADIUS = 64; // 0x14ce82c3b
    const char *const CAN_JOIN_RAID_COMPONENT = "minecraft:can_join_raid";
    const char *const SPAWN_FOR_RAID_EVENT = "minecraft:spawn_for_raid";
    // recruits are looked for in the recruiter's box grown by 16 on every axis (sub_1477465E0)
    const float RECRUIT_RANGE = 16.0f;

    std::shared_ptr<Village> raidedVillageAt(ServerNetworkHandler &owner, const MobActor &mob) {
        // the village whose own box holds the mob, without margin
        const Vector3f &position = mob.getPosition();
        const Vector3i origin((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                              (int32_t) std::floor(position.z));
        std::shared_ptr<Village> village = owner.getLevelFor(mob).getVillageManager().getClosestVillage(origin, 0);
        return village != nullptr && village->getRaid() != nullptr ? village : nullptr;
    }

    std::mt19937 &dwellerRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    DwellerRole roleOf(const json::Value *value) {
        const std::string name = value == nullptr ? std::string() : value->string();
        if (name == "inhabitant")
            return DwellerRole::Inhabitant;
        if (name == "defender")
            return DwellerRole::Defender;
        if (name == "hostile")
            return DwellerRole::Hostile;
        if (name == "passive")
            return DwellerRole::Passive;
        return DwellerRole::None;
    }

    bool flagIn(const json::Value &component, const char *key) {
        const json::Value *value = component.get(key);
        return value != nullptr && value->boolean(false);
    }

}

void MobDweller::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const json::Value *component = mob.getComponent(DWELLER_COMPONENT);
    if (component != mComponent) {
        mComponent = component;
        if (component != nullptr)
            _read(*component);
    }

    if (mComponent != nullptr && !mob.isExpired() && mob.isAlive()) {
        // the raid recruitment runs for a hostile dweller on the tick its update comes due
        if (mUpdateCountdown == 0 && mRole == DwellerRole::Hostile)
            _recruitForRaid(owner, mob);

        if (mUpdateCountdown == 0)
            _updateVillage(owner, mob);

        // the countdown set by an update is already decremented on the same tick
        if (mUpdateCountdown != 0)
            --mUpdateCountdown;
    }

    if (mRaidJoinQueued)
        _tryJoinRaid(owner, mob);
}

void MobDweller::onDeath(const MobActor &mob, const ServerPlayer *killer) {
    const std::shared_ptr<Village> village = mVillage.lock();
    if (village != nullptr) {
        Raid *raid = village->getRaid();
        if (mRole == DwellerRole::Hostile && killer != nullptr && raid != nullptr)
            raid->addHero(killer->getUniqueId());
        village->removeDweller(mRole, mob.getUniqueId());
    }
    mVillage.reset();
}

void MobDweller::joinVillage(ServerNetworkHandler &owner, const MobActor &mob, Village &village) {
    // the raid_configuration group adding the component may have come after the last tick read it
    const json::Value *component = mob.getComponent(DWELLER_COMPONENT);
    if (component != mComponent) {
        mComponent = component;
        if (component != nullptr)
            _read(*component);
    }
    if (mComponent == nullptr || mRole == DwellerRole::None)
        return;

    const std::shared_ptr<Village> current = mVillage.lock();
    if (current.get() == &village)
        return;

    const int64_t id = mob.getUniqueId();
    if (current != nullptr)
        current->removeDweller(mRole, id);
    village.addDweller(owner, mRole, id);
    mVillage = village.shared_from_this();
    mHasJoinedVillage = true;
}

void MobDweller::_read(const json::Value &component) {
    mRole = roleOf(component.get("dweller_role"));
    mCanFindPoi = flagIn(component, "can_find_poi");
    mCanMigrate = flagIn(component, "can_migrate");
    const json::Value *base = component.get("update_interval_base");
    mUpdateIntervalBase = base == nullptr ? 0 : base->integer(0);
    const json::Value *variant = component.get("update_interval_variant");
    mUpdateIntervalVariant = variant == nullptr ? 0 : variant->integer(0);
    const json::Value *tolerance = component.get("dwelling_bounds_tolerance");
    mBoundsTolerance = tolerance == nullptr ? DEFAULT_BOUNDS_TOLERANCE
                                            : (float) tolerance->number(DEFAULT_BOUNDS_TOLERANCE);
}

void MobDweller::_updateVillage(ServerNetworkHandler &owner, MobActor &mob) {
    VillageManager &villages = owner.getLevelFor(mob).getVillageManager();
    const int64_t id = mob.getUniqueId();
    if (mCanFindPoi)
        villages.requestScan(id);

    const int32_t roll = mUpdateIntervalVariant > 0
                         ? std::uniform_int_distribution<int32_t>(0, mUpdateIntervalVariant - 1)(dwellerRandom())
                         : 0;
    mUpdateCountdown = mUpdateIntervalBase + roll;

    // the game saves the dweller's village with the actor; a reloaded dweller finds it again here: joining anew
    // would drop its claims without releasing them
    if (!mHasJoinedVillage) {
        mVillage = villages.getDwellerVillage(mRole, id);
        mHasJoinedVillage = !mVillage.expired();
    }

    const std::shared_ptr<Village> current = mVillage.lock();
    const Vector3f &position = mob.getPosition();
    const bool mayMove = mCanMigrate || !mHasJoinedVillage;
    if (mayMove && (current == nullptr || !current->contains(mBoundsTolerance, position))) {
        const Vector3i origin((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                              (int32_t) std::floor(position.z));
        const std::shared_ptr<Village> next = villages.getClosestVillageForDweller(origin, MIGRATION_RADIUS);
        if (next != nullptr && next != current) {
            if (current != nullptr)
                current->removeDweller(mRole, id);
            next->addDweller(owner, mRole, id);
            mVillage = next;
            mHasJoinedVillage = true;
        }
    } else if (current != nullptr) {
        current->markDwellerSeen(mRole, id);
    }

    const std::shared_ptr<Village> joined = mVillage.lock();
    if (joined == nullptr || !joined->hasDweller(mRole, id))
        mVillage.reset();
}

void MobDweller::_recruitForRaid(ServerNetworkHandler &owner, const MobActor &mob) const {
    // sub_1477465E0: a raider of a village under raid, standing in that village, calls the mobs around it that can
    // join a raid and are not queued or raiding already; a won or lost raid still recruits while it celebrates
    const std::shared_ptr<Village> village = raidedVillageAt(owner, mob);
    if (village == nullptr || !village->hasDweller(DwellerRole::Hostile, mob.getUniqueId()))
        return;

    const Raid &raid = *village->getRaid();
    const AxisAlignedBB area = ActorPushSystem::boundingBoxOf(mob).expand(RECRUIT_RANGE, RECRUIT_RANGE, RECRUIT_RANGE);
    // the whole width of the area reaches any candidate whose own box overlaps it
    const float reach = std::max(area.mMaxX - area.mMinX, area.mMaxZ - area.mMinZ);
    std::vector<MobActor *> recruits;
    owner.forEachActorNear(mob.getPosition(), reach, [&](ServerActor &actor) {
        MobActor *candidate = dynamic_cast<MobActor *>(&actor);
        if (candidate == nullptr || candidate == &mob || !candidate->sharesLevelWith(mob) || candidate->isExpired()
            || !candidate->isAlive() || candidate->getComponent(CAN_JOIN_RAID_COMPONENT) == nullptr
            || candidate->getDweller().isRaidJoinQueued() || raid.hasRaider(candidate->getUniqueId())
            || !ActorPushSystem::boundingBoxOf(*candidate).intersectsWith(area))
            return;
        recruits.push_back(candidate);
    });

    for (MobActor *recruit: recruits) {
        recruit->fireEvent(owner, SPAWN_FOR_RAID_EVENT);
        recruit->getDweller().queueRaidJoin();
    }
}

void MobDweller::_tryJoinRaid(ServerNetworkHandler &owner, const MobActor &mob) {
    // sub_14CE8F940: the recruit stays queued until it is a hostile dweller of the raided village it stands in, even
    // after that raid ends; only a successful join drops the request (sub_1484BC3F0)
    if (mob.isExpired())
        return;

    const std::shared_ptr<Village> village = raidedVillageAt(owner, mob);
    if (village == nullptr || !village->hasDweller(DwellerRole::Hostile, mob.getUniqueId()))
        return;

    village->getRaid()->addRecruit(mob.getUniqueId(), mob.getHealth());
    mRaidJoinQueued = false;
}
