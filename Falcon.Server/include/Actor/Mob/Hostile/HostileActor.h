#pragma once

#include "Actor/Mob/MobActor.h"

class HostileActor : public MobActor {
public:
    static constexpr int DEFAULT_EXPERIENCE = 5;

    using MobActor::MobActor;

    ActorCategory getCategory() const override { return ActorCategory::Hostile; }

    int getExperienceDrop() const override { return DEFAULT_EXPERIENCE; }

    bool preventsSleep() const override {
        return true;
    }
};
