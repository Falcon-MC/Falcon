#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Json/Json.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>
#include <memory>
#include <string>
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
        bool mCheckIfOutnumbered = false;
    };

    struct Options {
        bool mRemoveTarget = false;
        std::shared_ptr<json::Value> mOnEscape;
        std::string mSound;
        int32_t mMinSoundInterval = 1;
        int32_t mMaxSoundInterval = 1;
    };

    AvoidMobTypeGoal(std::vector<Entry> entries, Options options);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    const Actor *_findThreat(ServerNetworkHandler &owner, MobActor &mob, const Entry *&entry) const;

    bool _isOutnumbered(ServerNetworkHandler &owner, MobActor &mob, const Entry &entry) const;

    int32_t _nextSound() const;

    std::vector<Entry> mEntries;
    Options mOptions;
    const Entry *mEntry = nullptr;
    Vector3f mThreatPosition;
    Vector3f mDestination;
    bool mSprinting = false;
    int32_t mSoundTicks = 0;
};
