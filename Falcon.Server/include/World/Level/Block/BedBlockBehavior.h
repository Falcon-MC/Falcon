#pragma once

#include "World/Level/Block/Components/BlockBehavior.h"

#include <cmath>

class BedBlockBehavior : public BlockBehavior {
public:
    float getLandingBounce() const override {
        return 0.75f;
    }

    // beds halve the fall damage
    std::optional<float> getFallDamage(const Actor &actor, float vanillaFallDamage) const override {
        (void) actor;
        return std::ceil(vanillaFallDamage * 0.5f);
    }
};
