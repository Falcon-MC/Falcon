#pragma once

#include "World/Actor/AI/Goal/Goal.h"

#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace json {
    class Value;
}

// minecraft:behavior.send_event: casts one weighted choice at a nearby entity its filters accept, sending it events
class SendEventGoal : public Goal {
public:
    // base_delay in ticks
    struct Step {
        int32_t mBaseDelay = 0;
        std::string mEvent;
        std::string mSoundEvent;
    };

    struct Choice {
        float mMinRange = 1.0f;
        float mMaxRange = 32.0f;
        int32_t mCooldown = 0;
        int32_t mCastDuration = 0;
        float mWeight = 0.0f;
        bool mDoCasting = true;
        bool mLookAtTarget = true;
        // ARGB
        int32_t mParticleColor = -1;
        std::shared_ptr<json::Value> mFilters;
        std::string mStartSoundEvent;
        std::vector<Step> mSequence;
    };

    explicit SendEventGoal(std::vector<Choice> choices);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    MobActor *_receiver(ServerNetworkHandler &owner) const;

    std::vector<Choice> mChoices;
    std::mt19937 mRandom{std::random_device{}()};
    int32_t mChoice = -1;
    int64_t mReceiver = -1;
    int32_t mCastTicks = 0;
    int32_t mStep = 0;
    int32_t mDelayTicks = -1;
    int64_t mCooldownEndTick = 0;
};
