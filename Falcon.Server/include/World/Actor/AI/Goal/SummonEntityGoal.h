#pragma once

#include "Core/Math/Vector3f.h"
#include "World/Actor/AI/Goal/Goal.h"

#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace json {
    class Value;
}

// minecraft:behavior.summon_entity: casts one weighted choice of spawn sequences around the mob or its target
class SummonEntityGoal : public Goal {
public:
    enum class Shape { Circle, Line };

    // times in ticks
    struct Spawn {
        Shape mShape = Shape::Line;
        bool mAroundTarget = false;
        float mSize = 1.0f;
        int32_t mBaseDelay = 0;
        int32_t mDelayPerSummon = 0;
        int32_t mCount = 1;
        int32_t mLifespan = -20;
        int32_t mSummonCap = 0;
        float mSummonCapRadius = 0.0f;
        std::string mEntityType;
        std::string mSoundEvent;
        std::string mSummonEvent;
    };

    struct Choice {
        float mMinRange = 1.0f;
        float mMaxRange = 32.0f;
        int32_t mCooldown = 0;
        float mWeight = 0.0f;
        bool mDoCasting = true;
        // ARGB
        int32_t mParticleColor = -1;
        int32_t mCastDuration = 0;
        std::shared_ptr<json::Value> mFilters;
        std::string mStartSoundEvent;
        std::vector<Spawn> mSequence;
    };

    explicit SummonEntityGoal(std::vector<Choice> choices);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    bool _capped(ServerNetworkHandler &owner, MobActor &mob, const Choice &choice) const;

    void _spawnOne(ServerNetworkHandler &owner, MobActor &mob, const Spawn &spawn, float x, float z, float bottom,
                   float top);

    void _setCasting(ServerNetworkHandler &owner, MobActor &mob, bool casting);

    std::vector<Choice> mChoices;
    std::mt19937 mRandom{std::random_device{}()};
    int32_t mChoice = -1;
    int32_t mCastTicks = 0;
    int32_t mStep = 0;
    int32_t mSpawned = 0;
    int32_t mDelayTicks = -1;
    int64_t mCooldownEndTick = 0;
    Vector3f mTargetStart;
    Vector3f mSelfStart;
};
