#pragma once

#include "World/Actor/AI/Goal/MoveToPoiGoal.h"
#include "Core/Json/Json.h"

#include <cstdint>
#include <memory>

// minecraft:behavior.work: a villager walks to its job site and works there, which its village counts
class WorkGoal : public MoveToPoiGoal {
public:
    struct Settings {
        float mSpeed = 0.0f;
        int32_t mActiveTicks = 0;
        bool mCanWorkInRain = false;
        // the last ticks of the shift worked through the rain; -1 for none
        int32_t mRainToleranceTicks = -1;
        int32_t mCooldownTicks = 0;
        int32_t mSoundDelayMin = 0;
        int32_t mSoundDelayMax = 0;
        std::shared_ptr<json::Value> mOnArrival;
    };

    explicit WorkGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

    int32_t getWorkedTicks() const {
        return mWorkedTicks;
    }

private:
    bool _isRainingOnSite(ServerNetworkHandler &owner, const MobActor &mob) const;

    int32_t _nextSoundDelay() const;

    Settings mSettings;
    int32_t mCooldownLeft = 0;
    int32_t mWorkedTicks = 0;
    int32_t mSoundTicks = 0;
    int32_t mSoundDelay = 0;
    std::string mSound;
    bool mArrived = false;
};
