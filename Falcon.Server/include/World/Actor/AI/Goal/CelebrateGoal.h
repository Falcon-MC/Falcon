#pragma once

#include "World/Actor/AI/Goal/Goal.h"
#include "Core/Json/Json.h"

#include <cstdint>
#include <memory>
#include <string>

// minecraft:behavior.celebrate: random jumps and sounds while targetless, then on_celebration_end_event
class CelebrateGoal : public Goal {
public:
    struct Settings {
        std::string mSound;
        float mSoundIntervalMin = 0.0f;
        float mSoundIntervalMax = 0.0f;
        float mJumpIntervalMin = 0.0f;
        float mJumpIntervalMax = 0.0f;
        float mDuration = 0.0f;
        std::shared_ptr<json::Value> mOnEnd;
    };

    explicit CelebrateGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    static int32_t _rollTicks(float minimum, float maximum);

    static void _setCelebrating(ServerNetworkHandler &owner, MobActor &mob, bool value);

    Settings mSettings;
    int32_t mJumpTicks = 0;
    int32_t mSoundTicks = 0;
    int32_t mElapsedTicks = 0;
};
