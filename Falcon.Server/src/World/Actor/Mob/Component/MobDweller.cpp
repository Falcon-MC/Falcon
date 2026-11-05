#include "World/Actor/Mob/Component/MobDweller.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Level.h"
#include "World/Village/VillageManager.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>
#include <random>
#include <string>

namespace {
    const char *const DWELLER_COMPONENT = "minecraft:dweller";
    const float DEFAULT_BOUNDS_TOLERANCE = 10.0f; // 0x14efd22ac
    const int32_t MIGRATION_RADIUS = 64; // 0x14ce82c3b

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

    if (mComponent == nullptr || mob.isExpired() || !mob.isAlive())
        return;

    if (mUpdateCountdown == 0)
        _updateVillage(owner, mob);

    // the countdown set by an update is already decremented on the same tick
    if (mUpdateCountdown != 0)
        --mUpdateCountdown;
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
    // the raid_configuration group that adds the component may have been added after the last tick read it
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

    // the game saves the dweller's village with the actor; a reloaded dweller finds it again here instead of joining
    // anew, which would drop its claims without releasing them
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
