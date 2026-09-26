#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <string>
#include <unordered_set>

class TakeBlockGoal : public Goal {
public:
    struct Settings {
        std::unordered_set<std::string> mBlocks;
        float mChance = 0.0f;
        int32_t mMinXz = 0;
        int32_t mMaxXz = 0;
        int32_t mMinY = 0;
        int32_t mMaxY = 0;
    };

    explicit TakeBlockGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    Settings mSettings;
    Vector3i mPosition;
};
