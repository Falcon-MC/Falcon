#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Json/Json.h"

#include <cstdint>
#include <memory>
#include <string>

class AdmireItemGoal : public Goal {
public:
    AdmireItemGoal(std::string sound, int32_t minSoundInterval, int32_t maxSoundInterval,
                   std::shared_ptr<json::Value> onStart, std::shared_ptr<json::Value> onStop);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    int32_t _nextSound() const;

    std::string mSound;
    int32_t mMinSoundInterval;
    int32_t mMaxSoundInterval;
    std::shared_ptr<json::Value> mOnStart;
    std::shared_ptr<json::Value> mOnStop;
    int32_t mSoundTicks = 0;
};
