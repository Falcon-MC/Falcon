#pragma once

#include "Core/Json/Json.h"
#include "World/Village/Village.h"

#include <cstdint>
#include <memory>

class MobActor;
class ServerNetworkHandler;

/** `minecraft:dweller`: asks for point of interest scans and keeps the mob registered with the village it lives in. */
class MobDweller {
public:
    void tick(ServerNetworkHandler &owner, MobActor &mob);

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
