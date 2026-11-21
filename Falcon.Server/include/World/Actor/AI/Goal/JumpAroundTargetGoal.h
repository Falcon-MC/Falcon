#pragma once

#include "Core/Math/Vector3f.h"
#include "World/Actor/AI/Goal/Goal.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <vector>

namespace json {
    class Value;
}

class Actor;
struct AxisAlignedBB;
class Level;

// minecraft:behavior.jump_around_target: the breeze's long jumps to a spot around its target
class JumpAroundTargetGoal : public Goal {
public:
    // times in ticks
    struct Settings {
        std::shared_ptr<json::Value> mFilters;
        int32_t mCooldown = 10;
        int32_t mHurtCooldown = 2;
        int32_t mLastHurtDuration = 40;
        int32_t mPrepareDuration = 10;
        float mMaxJumpVelocity = 1.4f;
        float mBoxScale = 0.7f;
        bool mCheckCollision = false;
        int32_t mLineOfSightIgnoredHeight = 4;
        float mMinTargetDistance = 4.0f;
        float mMaxTargetDistance = 20.0f;
        float mLandingSpreadDegrees = 90.0f;
        float mMinLandingDistance = 4.0f;
        float mMaxLandingDistance = 8.0f;
        int32_t mRequiredVerticalSpace = 4;
        int32_t mSnapRange = 10;
        std::vector<float> mJumpAngles;
    };

    struct Jump {
        Vector3f mLanding;
        Vector3f mVelocity;
    };

    explicit JumpAroundTargetGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

    // a spot around the target, behind the way its head faces within the spread, as far as the range draws
    static Vector3f positionAroundTarget(const Actor &target, float spreadDegrees, float minDistance,
                                         float maxDistance, std::mt19937 &random);

    // the launch velocity of a jump at that angle from one point to another, under max velocity; with
    // checkCollision, the box shrunk by boxScale must clear every block along the arc
    static std::optional<Vector3f> launchVelocity(Level &level, const AxisAlignedBB &box, const Vector3f &from,
                                                  const Vector3f &to, float angleDegrees, float maxVelocity,
                                                  float boxScale, bool checkCollision);

private:
    enum class Phase { Prepare, Jumping, Done };

    std::optional<Jump> _plan(ServerNetworkHandler &owner, MobActor &mob, const Vector3f &landing);

    Settings mSettings;
    std::mt19937 mRandom;
    Phase mPhase = Phase::Done;
    int64_t mStartTick = 0;
    int64_t mCooldownEnd = 0;
    int64_t mWaterLandingTick = 0;
    std::optional<Jump> mJump;
    bool mHadDiscardFriction = false;
};
