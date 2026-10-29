#pragma once

#include "World/Actor/Mob/MobActor.h"

class HostileActor : public MobActor {
public:
    static constexpr int DEFAULT_EXPERIENCE = 5;

    using MobActor::MobActor;

    ActorCategory getCategory() const override { return ActorCategory::Hostile; }

    bool shouldRemoveInPeaceful() const override { return true; }

    int getExperienceDrop() const override { return DEFAULT_EXPERIENCE; }

    bool preventsSleep() const override {
        return true;
    }
};
