#pragma once

#include "Core/Json/Json.h"
#include "World/Village/Village.h"

#include <cstdint>
#include <memory>

class MobActor;
class ServerNetworkHandler;
class ServerPlayer;

/** `minecraft:dweller`: asks for point of interest scans and keeps the mob registered with the village it lives in. */
class MobDweller {
public:
    void tick(ServerNetworkHandler &owner, MobActor &mob);

    /**
     * Leaves the village on death, releasing everything the dweller claimed. A player who kills a hostile dweller of a
     * village under raid becomes one of the raid's heroes.
     */
    void onDeath(const MobActor &mob, const ServerPlayer *killer);

    /** Moves the dweller to the village, as raiders are bound to the village they raid. Ignored without the component. */
    void joinVillage(ServerNetworkHandler &owner, const MobActor &mob, Village &village);

    bool isActive() const {
        return mComponent != nullptr;
    }

    DwellerRole getRole() const {
        return mRole;
    }

private:
    void _read(const json::Value &component);

    void _updateVillage(ServerNetworkHandler &owner, MobActor &mob);

    const json::Value *mComponent = nullptr;
    DwellerRole mRole = DwellerRole::None;
    bool mCanFindPoi = false;
    bool mCanMigrate = false;
    bool mHasJoinedVillage = false;
    int64_t mUpdateIntervalBase = 0;
    int32_t mUpdateIntervalVariant = 0;
    float mBoundsTolerance = 0.0f;
    int64_t mUpdateCountdown = 0;
    std::weak_ptr<Village> mVillage;
};
