#pragma once

#include "Actor/Mob/MobActor.h"

class NeutralActor : public MobActor {
public:
    static constexpr int MINIMUM_EXPERIENCE = 1;

    static constexpr int MAXIMUM_EXPERIENCE = 3;

    using MobActor::MobActor;

    ActorCategory getCategory() const override { return ActorCategory::Neutral; }

    int getExperienceDrop() const override {
        return randomRange(MINIMUM_EXPERIENCE, MAXIMUM_EXPERIENCE);
    }
};
