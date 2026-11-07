#pragma once

#include "World/Level/Block/Components/BlockBehavior.h"

class BedBlockBehavior : public BlockBehavior {
public:
    float getLandingBounce() const override {
        return 0.75f;
    }

    // BedBlock::onFallOn (client 26.60 sub_1431540B0), whether or not the actor sneaks
    float getFallDistanceMultiplier() const override { return 0.5f; }
};
