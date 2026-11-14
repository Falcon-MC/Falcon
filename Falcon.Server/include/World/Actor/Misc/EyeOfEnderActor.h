#pragma once

#include "World/Actor/SizedActor.h"

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <random>

// a thrown eye of ender: it climbs towards a point at most 12 blocks along the way to the stronghold and, after
// 80 ticks, drops back as an item or shatters; the client draws its trail from the motion it is sent
class EyeOfEnderActor final : public SizedActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:eye_of_ender_signal";

    static constexpr int32_t LIFETIME = 80;

    EyeOfEnderActor(uint64_t runtimeId, const std::string &identifier)
            : SizedActor(runtimeId, identifier, ActorSize{0.25f, 0.25f}, false) {}

    // aimed at the block, the chance to survive drawn from the random
    void signalTo(const Vector3i &target, std::mt19937 &random);

    // one flight step: the position moves by the motion, then the motion turns towards the target
    static void step(Vector3f &position, Vector3f &motion, const Vector3f &target);

    const Vector3f &getTarget() const noexcept {
        return mTarget;
    }

    bool survivesDeath() const noexcept {
        return mSurviveAfterDeath;
    }

    bool hasGravity() const override {
        return false;
    }

    bool isPushable() const override {
        return false;
    }

    // its target is not part of the save: a reloaded eye would lose its way
    bool shouldSave() const override {
        return false;
    }

    bool isExpired() const override {
        return mExpired;
    }

    void tick(ServerNetworkHandler &owner) override;

private:
    Vector3f mTarget;
    int32_t mLife = 0;
    bool mSurviveAfterDeath = false;
    bool mExpired = false;
};
