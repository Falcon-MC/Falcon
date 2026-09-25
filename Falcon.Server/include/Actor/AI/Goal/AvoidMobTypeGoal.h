#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Json/Json.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>
#include <memory>
#include <vector>

class Actor;

class AvoidMobTypeGoal : public Goal {
public:
    struct Entry {
        std::shared_ptr<json::Value> mFilters;
        float mMaxDistance = 3.0f;
        float mWalkSpeed = 0.25f;
        float mSprintSpeed = 0.25f;
        float mSprintDistance = 7.0f;
    };

    explicit AvoidMobTypeGoal(std::vector<Entry> entries);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    const Actor *_findThreat(ServerNetworkHandler &owner, MobActor &mob, const Entry *&entry) const;

    std::vector<Entry> mEntries;
    const Entry *mEntry = nullptr;
    Vector3f mThreatPosition;
    Vector3f mDestination;
    bool mSprinting = false;
};
