#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Actor/ActorFlags.h"
#include "Core/Json/Json.h"

#include <cstdint>
#include <memory>

class TimerFlagGoal : public Goal {
public:
    TimerFlagGoal(ActorFlag flag, int32_t minDuration, int32_t maxDuration, int32_t minCooldown,
                  int32_t maxCooldown, std::shared_ptr<json::Value> onStart, std::shared_ptr<json::Value> onEnd,
                  uint8_t controlFlags);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    static int32_t _roll(int32_t minimum, int32_t maximum);

    void _setFlag(ServerNetworkHandler &owner, MobActor &mob, bool value) const;

    ActorFlag mFlag;
    int32_t mMinDuration;
    int32_t mMaxDuration;
    int32_t mMinCooldown;
    int32_t mMaxCooldown;
    std::shared_ptr<json::Value> mOnStart;
    std::shared_ptr<json::Value> mOnEnd;
    int64_t mEndTick = 0;
    int64_t mCooldownEndTick = 0;
};
