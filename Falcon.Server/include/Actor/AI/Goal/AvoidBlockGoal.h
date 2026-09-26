#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Json/Json.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>

class AvoidBlockGoal : public Goal {
public:
    struct Settings {
        int32_t mTickInterval = 1;
        int32_t mSearchRange = 0;
        int32_t mSearchHeight = 0;
        float mSprintSpeed = 0.25f;
        bool mRandomTarget = false;
        std::unordered_set<std::string> mBlocks;
        std::string mSound;
        int32_t mMinSoundInterval = 1;
        int32_t mMaxSoundInterval = 1;
        std::shared_ptr<json::Value> mOnEscape;
    };

    explicit AvoidBlockGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    bool _findBlock(ServerNetworkHandler &owner, const MobActor &mob, Vector3f &block) const;

    int32_t _nextSound() const;

    Settings mSettings;
    int32_t mScanTicks = 0;
    int32_t mSoundTicks = 0;
    Vector3f mDestination;
};
