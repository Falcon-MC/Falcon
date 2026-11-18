#pragma once

#include "World/Level/Block/Components/BlockBehavior.h"

class BedBlockBehavior : public BlockBehavior {
public:
    float getLandingBounce() const override {
        return 0.75f;
    }

    // whether or not the actor sneaks
    float getFallDistanceMultiplier() const override {
        return 0.5f;
    }
};
