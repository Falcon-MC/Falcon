#pragma once

#include "Core/Json/Json.h"
#include "World/Actor/AI/Goal/Goal.h"

#include <cstdint>
#include <memory>

class Actor;

// a patrol stops and watches a target still far away, and tells the others of its kind around it
class HoldGroundGoal : public Goal {
public:
    struct Settings {
        float mMinRadius = 10.0f;
        bool mBroadcast = false;
        float mBroadcastRange = 0.0f;
        std::shared_ptr<json::Value> mWithinRadiusEvent;
    };

    explicit HoldGroundGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    Actor *_target(ServerNetworkHandler &owner, const MobActor &mob) const;

    template<typename Visitor>
    void _forEachKin(ServerNetworkHandler &owner, MobActor &mob, bool includeSelf, Visitor &&visit) const;

    Settings mSettings;
    uint64_t mTargetId = 0;
};
