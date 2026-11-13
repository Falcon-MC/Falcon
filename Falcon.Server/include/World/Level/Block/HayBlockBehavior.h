#pragma once

#include "World/Level/Block/Components/BlockBehavior.h"

class HayBlockBehavior final : public BlockBehavior {
public:
    float getFallDistanceMultiplier() const override {
        return 0.2f;
    }
};
