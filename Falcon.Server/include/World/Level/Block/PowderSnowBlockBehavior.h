#pragma once

#include "World/Level/Block/Components/BlockBehavior.h"

class PowderSnowBlockBehavior final : public BlockBehavior {
public:
    // sub_148F8E0B0: landing on powder snow clears the fall distance
    float getFallDistanceMultiplier() const override {
        return 0.0f;
    }
};
