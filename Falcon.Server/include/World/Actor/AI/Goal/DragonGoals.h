#pragma once

#include "World/Actor/AI/Goal/Goal.h"
#include "World/Actor/Mob/Hostile/EnderDragon.h"

#include <memory>
#include <string>

// minecraft:behavior.dragon*: a goal starts only while its component group is on the dragon;
// an event swapping the groups rebuilds the goals on the next tick
class DragonGoal : public Goal {
public:
    explicit DragonGoal(const char *group);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) final;

protected:
    virtual bool canStart(ServerNetworkHandler &owner, EnderDragon &dragon) = 0;

    static EnderDragon &dragonOf(MobActor &mob);

    // random height up to 20 blocks above the node
    static void navigate(EnderDragon &dragon, EnderDragon::Path *path);

    // the node a ring walk ends on: the outer ring while crystals stand, the inner ones once they are gone
    static int32_t ringNode(int32_t node, int32_t crystals);

    static bool needsNewTarget(const EnderDragon &dragon);

    static bool hasTarget(ServerNetworkHandler &owner, EnderDragon &dragon);

private:
    const char *mGroup;
};

// sub_14D1D0480: circles the island, decides when to land or chase a player
class DragonHoldingPatternGoal final : public DragonGoal {
public:
    DragonHoldingPatternGoal();

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

protected:
    bool canStart(ServerNetworkHandler &owner, EnderDragon &dragon) override;

private:
    void _findNewTarget(ServerNetworkHandler &owner, EnderDragon &dragon);

    std::unique_ptr<EnderDragon::Path> mPath;
    bool mClockwise = false;
};

// sub_14D1D2E50: fireball once it faces the target
class DragonStrafePlayerGoal final : public DragonGoal {
public:
    struct Settings {
        float mActiveSpeed = 0.6f;
        float mFlightSpeed = 0.6f;
        float mTurnSpeed = 0.7f;
        float mMinDistance = 10.0f;
        float mMaxDistance = 150.0f;
        int32_t mInViewTicks = 5;
        float mFireballRange = 64.0f;
        float mSwitchDirectionProbability = 0.125f;
        float mViewAngle = 10.0f;
    };

    explicit DragonStrafePlayerGoal(const Settings &settings);

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

protected:
    bool canStart(ServerNetworkHandler &owner, EnderDragon &dragon) override;

private:
    void _findNewTarget(ServerNetworkHandler &owner, EnderDragon &dragon);

    Settings mSettings;
    std::unique_ptr<EnderDragon::Path> mPath;
    uint64_t mTarget = 0;
    bool mClockwise = false;
    bool mFired = false;
    int32_t mInView = 0;
};

// sub_14D1CEA20: dives at the nearest player once it leaves the portal to charge
class DragonChargePlayerGoal final : public DragonGoal {
public:
    struct Settings {
        float mActiveSpeed = 3.0f;
        float mFlightSpeed = 0.6f;
        float mTurnSpeed = 0.7f;
        float mMinDistance = 10.0f;
        float mMaxDistance = 150.0f;
        int32_t mContinueTicks = 10;
    };

    explicit DragonChargePlayerGoal(const Settings &settings);

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

protected:
    bool canStart(ServerNetworkHandler &owner, EnderDragon &dragon) override;

private:
    Settings mSettings;
    int32_t mTimer = 0;
};

// sub_14D1D1110: follows the nodes down to the exit portal
class DragonLandingGoal final : public DragonGoal {
public:
    DragonLandingGoal();

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

protected:
    bool canStart(ServerNetworkHandler &owner, EnderDragon &dragon) override;

private:
    std::unique_ptr<EnderDragon::Path> mPath;
    bool mFinalApproach = false;
};

// sub_14D1CF740: roars, then breathes dragon's breath in front of the portal
class DragonFlamingGoal final : public DragonGoal {
public:
    struct Settings {
        int32_t mGroundFlameCount = 4;
        int32_t mCooldownTicks = 200;
        int32_t mFlameTicks = 10;
        int32_t mRoarTicks = 40;
        int32_t mSmokeTicks = 200;
        float mSmokeRadius = 5.0f;
        int32_t mSmokeColor = (int32_t) 0xFFDC00EF;
    };

    explicit DragonFlamingGoal(const Settings &settings);

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

protected:
    bool canStart(ServerNetworkHandler &owner, EnderDragon &dragon) override;

private:
    void _breathe(ServerNetworkHandler &owner, EnderDragon &dragon);

    Settings mSettings;
    int32_t mTicks = 0;
    int32_t mRoar = 0;
};

// sub_14D1D1B20, sub_14D1D2130: looks for a player to flame or charge
class DragonScanningGoal final : public DragonGoal {
public:
    DragonScanningGoal();

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

protected:
    bool canStart(ServerNetworkHandler &owner, EnderDragon &dragon) override;

private:
    float mScanTime = 0.0f;
};

// sub_14D1D3B50: leaves toward the node it looks at
class DragonTakeoffGoal final : public DragonGoal {
public:
    DragonTakeoffGoal();

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

protected:
    bool canStart(ServerNetworkHandler &owner, EnderDragon &dragon) override;

private:
    std::unique_ptr<EnderDragon::Path> mPath;
    bool mFirstTick = false;
};

// sub_14D1CED00: flies back over the portal to die there
class DragonDeathGoal final : public DragonGoal {
public:
    DragonDeathGoal();

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

protected:
    bool canStart(ServerNetworkHandler &owner, EnderDragon &dragon) override;

private:
    int32_t mTicks = 0;
};

class DragonGoals {
public:
    // nullptr for another behavior or mob
    static std::unique_ptr<Goal> create(const MobActor &mob, const std::string &behavior, const json::Value &component);
};
