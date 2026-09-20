#pragma once

#include "Actor/Mob/Hostile/HostileActor.h"

class AbstractSlimeActor : public HostileActor {
public:
    static constexpr int LARGE_SIZE = 4;

    static constexpr int MEDIUM_SIZE = 2;

    static constexpr int SMALL_SIZE = 1;

    static constexpr float SIZE_SCALE = 0.51f;

    using HostileActor::HostileActor;

    int getSizeVariant() const { return mSizeVariant; }

    void setSizeVariant(int variant) { mSizeVariant = variant; }

    ActorSize getSize() const override {
        const float extent = SIZE_SCALE * (float) mSizeVariant;
        return ActorSize{extent, extent};
    }

    float getDefaultMaxHealth() const override {
        return (float) (mSizeVariant * mSizeVariant);
    }

    int getExperienceDrop() const override { return 0; }

private:
    int mSizeVariant = LARGE_SIZE;
};
