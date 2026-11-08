#pragma once

#include "Core/Json/Json.h"
#include "World/Village/Village.h"

#include <cstdint>
#include <memory>

class MobActor;
class ServerNetworkHandler;
class ServerPlayer;

// minecraft:dweller: requests poi scans, keeps the mob registered with its village
class MobDweller {
public:
    void tick(ServerNetworkHandler &owner, MobActor &mob);

    // releases everything the dweller claimed; killing a hostile dweller of a raided village makes the killer a hero
    void onDeath(const MobActor &mob, const ServerPlayer *killer);

    // raiders are bound to the village they raid; no-op without the component
    void joinVillage(ServerNetworkHandler &owner, const MobActor &mob, Village &village);

    // joins once it is a hostile dweller of a raided village
    void queueRaidJoin() {
        mRaidJoinQueued = true;
    }

    bool isRaidJoinQueued() const {
        return mRaidJoinQueued;
    }

    bool isActive() const {
        return mComponent != nullptr;
    }

    DwellerRole getRole() const {
        return mRole;
    }

    std::shared_ptr<Village> getVillage() const {
        return mVillage.lock();
    }

private:
    void _read(const json::Value &component);

    void _updateVillage(ServerNetworkHandler &owner, MobActor &mob);

    void _recruitForRaid(ServerNetworkHandler &owner, const MobActor &mob) const;

    void _tryJoinRaid(ServerNetworkHandler &owner, const MobActor &mob);

    const json::Value *mComponent = nullptr;
    DwellerRole mRole = DwellerRole::None;
    bool mCanFindPoi = false;
    bool mCanMigrate = false;
    bool mHasJoinedVillage = false;
    bool mRaidJoinQueued = false;
    int64_t mUpdateIntervalBase = 0;
    int32_t mUpdateIntervalVariant = 0;
    float mBoundsTolerance = 0.0f;
    int64_t mUpdateCountdown = 0;
    std::weak_ptr<Village> mVillage;
};
