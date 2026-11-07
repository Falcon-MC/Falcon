#pragma once

#include "World/Level/Block/Components/BlockBehavior.h"

class HayBlockBehavior final : public BlockBehavior {
public:
    // HayBlock::onFallOn (client 26.60 sub_146D7A680)
    float getFallDistanceMultiplier() const override {
        return 0.2f;
    }
};
