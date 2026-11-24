#pragma once

#include "Core/Math/Vector3f.h"
#include "World/Actor/AI/Goal/Goal.h"

#include <cstdint>
#include <memory>
#include <string>

namespace json {
    class Value;
}

class Actor;

// minecraft:behavior.fire_at_target: the breeze's shot, aimed from an anchor of the shooter at an anchor of its target
class FireAtTargetGoal : public Goal {
public:
    // anchors as the definition numbers them: 0 the feet, 1 the eyes, 2 the middle of the box
    enum class Anchor : int32_t { Feet = 0, Eyes = 1, Middle = 2 };

    // times in ticks, ranges squared
    struct Settings {
        std::string mProjectile;
        std::shared_ptr<json::Value> mFilters;
        float mMinRangeSquared = 0.0f;
        float mMaxRangeSquared = 0.0f;
        int32_t mCooldown = 0;
        int32_t mPreShootDelay = 0;
        int32_t mPostShootDelay = 0;
        Anchor mOwnerAnchor = Anchor::Middle;
        Anchor mTargetAnchor = Anchor::Feet;
        // in parts of the box: x of its width, y of its height, z of its depth
        Vector3f mOwnerOffset;
        Vector3f mTargetOffset;
    };

    explicit FireAtTargetGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

    // the point an anchor and its offset give on an actor, the offset turned with the body yaw
    static Vector3f anchorPoint(const Actor &actor, Anchor anchor, const Vector3f &offset);

private:
    Settings mSettings;
    int64_t mCooldownEnd = 0;
    int64_t mShootTick = 0;
    int64_t mEndTick = 0;
    bool mFired = false;
};
