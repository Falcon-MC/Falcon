#pragma once

#include "Core/Json/Json.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>
#include <unordered_map>

class Level;
class MobActor;
class ServerNetworkHandler;
class ServerPlayer;

class LookedAtSensor {
public:
    void tick(ServerNetworkHandler &owner, MobActor &mob);

private:
    ServerPlayer *_findLooker(ServerNetworkHandler &owner, MobActor &mob, const json::Value &component,
                              int32_t interval);

    static bool _isLookingAt(Level &level, const MobActor &mob, const ServerPlayer &player,
                             const json::Value &component);

    static bool _canSee(Level &level, const Vector3f &eye, const Vector3f &direction, const Vector3f &point,
                        float tolerance, bool scaleByDistance);

    const json::Value *mComponent = nullptr;
    int64_t mNextScanTick = 0;
    bool mStopped = false;
    std::unordered_map<uint64_t, int32_t> mLookTicks;
};
